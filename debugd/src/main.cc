// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <brillo/syslog_logging.h>
#include <chromeos/libminijail.h>

#include "debugd/src/debug_daemon.h"

namespace {

// @brief Enter a VFS namespace.
//
// We don't want anyone other than our descendants to see our tmpfs.
void enter_vfs_namespace() {
  struct minijail* j = minijail_new();
  minijail_namespace_vfs(j);
  minijail_enter(j);
  minijail_destroy(j);
}

// @brief Sets up a tmpfs visible to this program and its descendants.
//
// The created tmpfs is mounted at /debugd.
void make_tmpfs() {
  int r = mount("none", "/debugd", "tmpfs", MS_NODEV | MS_NOSUID | MS_NOEXEC,
                nullptr);
  if (r < 0)
    PLOG(FATAL) << "mount() failed";
}

// @brief Sets up directories needed by helper programs.
//
void setup_dirs() {
  int r = mkdir("/debugd/touchpad", S_IRWXU);
  if (r < 0)
    PLOG(FATAL) << "mkdir(\"/debugd/touchpad\") failed";
}

// @brief Start the debugd DBus interface.
void start() {
  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  DBus::Connection conn = DBus::Connection::SystemBus();
  debugd::DebugDaemon debugd(&conn, &dispatcher);
  if (!debugd.Init())
    LOG(FATAL) << "debugd.Init() failed";
  debugd.Run();
  LOG(FATAL) << "debugd.Run() returned";
}

}  // namespace

int __attribute__((visibility("default"))) main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);
  enter_vfs_namespace();
  make_tmpfs();
  setup_dirs();
  start();
  return 0;
}
