// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/process_executor.h"

#include <stdlib.h>
#include <algorithm>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>
#include <libminijail.h>
#include <scoped_minijail.h>

#include "authpolicy/log_level.h"
#include "authpolicy/platform_helper.h"

namespace authpolicy {
namespace {

// Splits string into lines and logs the lines. This works around a restriction
// of syslog of 8kb per log and fixes unreadable logs where \n is replaced by
// #012.
void LogLongString(const char* header, const std::string& str) {
  if (!(kLogExecutorOutput && LOG_IS_ON(INFO)))
    return;

  std::vector<std::string> lines = base::SplitString(
      str, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (lines.size() <= 1) {
    LOG(INFO) << header << str;
  } else {
    LOG(INFO) << header;
    for (const std::string& line : lines)
      LOG(INFO) << line;
  }
}

}  // namespace

ProcessExecutor::ProcessExecutor(std::vector<std::string> args)
    : args_(std::move(args)) {}

void ProcessExecutor::SetInputFile(int fd) {
  input_fd_ = fd;
}

void ProcessExecutor::SetInputString(const std::string& input_str) {
  input_str_ = input_str;
}

void ProcessExecutor::SetEnv(const std::string& key, const std::string& value) {
  env_map_[key] = value;
}

void ProcessExecutor::SetSeccompFilter(const std::string& policy_file) {
  seccomp_policy_file_ = policy_file;
}

void ProcessExecutor::LogSeccompFilterFailures() {
  log_seccomp_failures_ = true;
}

void ProcessExecutor::SetNoNewPrivs() {
  no_new_privs_ = true;
}

void ProcessExecutor::KeepSupplementaryGroups() {
  keep_supplementary_flags_ = true;
}

bool ProcessExecutor::Execute() {
  ResetOutput();
  if (args_.empty() || args_[0].empty())
    return true;

  if (!base::FilePath(args_[0]).IsAbsolute()) {
    LOG(ERROR) << "Command must be specified by absolute path.";
    exit_code_ = kExitCodeInternalError;
    return false;
  }

  if (kLogExecutorCommand && LOG_IS_ON(INFO)) {
    std::string cmd = args_[0];
    for (size_t n = 1; n < args_.size(); ++n)
      cmd += base::StringPrintf(" '%s'", args_[n].c_str());
    LOG(INFO) << "Executing " << cmd;
  }

  // Convert args to array of pointers. Must be nullptr terminated.
  std::vector<char*> args_ptr;
  for (const auto& arg : args_)
    args_ptr.push_back(const_cast<char*>(arg.c_str()));
  args_ptr.push_back(nullptr);

  // Save old environment and set ours. Note that clearenv() doesn't actually
  // delete any pointers, so we can just keep the old pointers.
  std::vector<char*> old_environ;
  for (char** env = environ; env != nullptr && *env != nullptr; ++env)
    old_environ.push_back(*env);
  clearenv();
  std::vector<std::string> env_list;
  for (const auto& env : env_map_) {
    env_list.push_back(env.first + "=" + env.second);
    putenv(const_cast<char*>(env_list.back().c_str()));
  }

  // Prepare minijail.
  ScopedMinijail jail(minijail_new());
  if (log_seccomp_failures_)
    minijail_log_seccomp_filter_failures(jail.get());
  if (!seccomp_policy_file_.empty()) {
    minijail_parse_seccomp_filters(jail.get(), seccomp_policy_file_.c_str());
    minijail_use_seccomp_filter(jail.get());
  }
  if (no_new_privs_)
    minijail_no_new_privs(jail.get());
  if (keep_supplementary_flags_)
    minijail_keep_supplementary_gids(jail.get());

  // Execute the command.
  pid_t pid = -1;
  int child_stdin = -1, child_stdout = -1, child_stderr = -1;
  minijail_run_pid_pipes(jail.get(),
                         args_ptr[0],
                         args_ptr.data(),
                         &pid,
                         &child_stdin,
                         &child_stdout,
                         &child_stderr);

  // Make sure the pipes never block.
  if (!base::SetNonBlocking(child_stdin))
    LOG(WARNING) << "Failed to set stdin non-blocking";
  if (!base::SetNonBlocking(child_stdout))
    LOG(WARNING) << "Failed to set stdout non-blocking";
  if (!base::SetNonBlocking(child_stderr))
    LOG(WARNING) << "Failed to set stderr non-blocking";

  // Restore the environment.
  clearenv();
  for (char* env : old_environ)
    putenv(env);

  // Write to child_stdin and read from child_stdout and child_stderr while
  // there is still data to read/write.
  bool io_success = PerformPipeIo(child_stdin,
                                  child_stdout,
                                  child_stderr,
                                  input_fd_,
                                  input_str_,
                                  &out_data_,
                                  &err_data_);

  // Wait for the process to exit.
  exit_code_ = minijail_wait(jail.get());
  jail.reset();

  // Print out a useful error message for seccomp failures.
  if (exit_code_ == MINIJAIL_ERR_JAIL)
    LOG(ERROR) << "Seccomp filter blocked a system call";

  // Always exit AFTER minijail_wait! If we do it before, the exit code is never
  // queried and the process is left dangling.
  if (!io_success) {
    LOG(ERROR) << "IO failed";
    exit_code_ = kExitCodeInternalError;
    return false;
  }

  LogLongString("Stdout: ", out_data_);
  LogLongString("Stderr: ", err_data_);
  LOG_IF(INFO, kLogExecutorCommand) << "Exit code: " << exit_code_;

  return exit_code_ == 0;
}

void ProcessExecutor::ResetOutput() {
  exit_code_ = 0;
  out_data_.clear();
  err_data_.clear();
}

}  // namespace authpolicy
