// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub implementation of Samba net. Does not talk to server, but simply returns
// fixed responses to predefined input.

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

#include "authpolicy/samba_helper.h"
#include "authpolicy/stub_common.h"

namespace authpolicy {
namespace {

// smbclient sub-commands.
const char kLcdCommand[] = "lcd ";
const char kCdCommand[] = "cd ";
const char kGetCommand[] = "get ";

// Error printed when a "remote" GPO fails to download.
const char kGpoDownloadError[] = "NT_STATUS_ACCESS_DENIED opening remote file ";

// Error printed when a "remote" GPO file does not exist.
const char kGpoDoesNotExistError[] =
    "NT_STATUS_OBJECT_NAME_NOT_FOUND opening remote file ";

struct DownloadItem {
  std::string remote_path_;
  std::string local_path_;
};

// Finds all 'cd <remote_dir>;lcd <local_dir>;get <filename>' triplets in an
// smbclient command and returns a list the concatenated local and remote file
// paths found.
std::vector<DownloadItem> GetDownloadItems(const std::string& command_line) {
  std::string remote_dir, local_dir;
  std::vector<DownloadItem> items;
  std::vector<std::string> subcommands = base::SplitString(
      command_line, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& subcommand : subcommands) {
    if (StartsWithCaseSensitive(subcommand, kCdCommand)) {
      remote_dir = subcommand.substr(strlen(kCdCommand));
    } else if (StartsWithCaseSensitive(subcommand, kLcdCommand)) {
      local_dir = subcommand.substr(strlen(kLcdCommand));
    } else if (StartsWithCaseSensitive(subcommand, kGetCommand)) {
      const std::string filename = subcommand.substr(strlen(kGetCommand));
      DownloadItem item;
      item.remote_path_ = remote_dir + "\\" + filename;
      item.local_path_ = base::FilePath(local_dir).Append(filename).value();
      items.push_back(item);
    }
  }
  return items;
}

int HandleCommandLine(const std::string& command_line) {
  // Stub GPO files are written to krb5.conf's directory because it's hard to
  // pass a full file path from a unit test to a stub binary. Note that
  // environment variables are NOT passed through ProcessExecutor.
  const base::FilePath gpo_dir =
      base::FilePath(GetKrb5ConfFilePath()).DirName();

  int last_error = kExitCodeOk;
  std::vector<DownloadItem> items = GetDownloadItems(command_line);
  for (const DownloadItem& item : items) {
    base::FilePath source_path;
    bool download_error = false;
    if (Contains(item.local_path_, kGpo1Guid))
      source_path = gpo_dir.Append(kGpo1Filename);
    else if (Contains(item.local_path_, kGpo2Guid))
      source_path = gpo_dir.Append(kGpo2Filename);
    else if (Contains(item.local_path_, kErrorGpoGuid))
      download_error = true;
    else
      NOTREACHED();

    if (download_error) {
      // Print "download error" warning.
      WriteOutput(kGpoDownloadError + item.remote_path_, "");
      last_error = kExitCodeError;
    } else if (!base::PathExists(source_path)) {
      // Print "file does not exist" warning.
      WriteOutput(kGpoDoesNotExistError + item.remote_path_, "");
      last_error = kExitCodeError;
    } else {
      // "Download" the file.
      base::FilePath target_path(item.local_path_);
      CHECK(base::CopyFile(source_path, target_path));
      last_error = kExitCodeOk;
    }
  }

  // Smbclient always returns the last error of each sub-command.
  return last_error;
}

}  // namespace
}  // namespace authpolicy

int main(int argc, char* argv[]) {
  const std::string command_line = authpolicy::GetCommandLine(argc, argv);
  return authpolicy::HandleCommandLine(command_line);
}
