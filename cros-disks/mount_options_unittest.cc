// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mount_options.h"

#include <sys/mount.h>

#include <string>
#include <vector>

#include <base/macros.h>
#include <gtest/gtest.h>

using std::pair;
using std::string;
using std::vector;

namespace cros_disks {

class MountOptionsTest : public ::testing::Test {
 protected:
  MountOptions mount_options_;
};

TEST_F(MountOptionsTest, IsReadOnlyOptionSet) {
  // default construction
  EXPECT_TRUE(mount_options_.IsReadOnlyOptionSet());

  // options: ro
  vector<string> options = {"ro"};
  mount_options_.Initialize(options, false, "", "");
  EXPECT_TRUE(mount_options_.IsReadOnlyOptionSet());

  // options: rw
  options = {"rw"};
  mount_options_.Initialize(options, false, "", "");
  EXPECT_FALSE(mount_options_.IsReadOnlyOptionSet());
}

TEST_F(MountOptionsTest, SetReadOnlyOption) {
  string expected_string_default = "ro";
  string expected_string_initialize = "ro,nodev,noexec,nosuid";

  // default construction
  mount_options_.SetReadOnlyOption();
  EXPECT_EQ(expected_string_default, mount_options_.ToString());

  // options: ro
  vector<string> options = {"ro"};
  mount_options_.Initialize(options, false, "", "");
  mount_options_.SetReadOnlyOption();
  EXPECT_EQ(expected_string_initialize, mount_options_.ToString());

  // options: rw
  options = {"rw"};
  mount_options_.Initialize(options, false, "", "");
  mount_options_.SetReadOnlyOption();
  EXPECT_EQ(expected_string_initialize, mount_options_.ToString());
}

TEST_F(MountOptionsTest, ToString) {
  vector<string> options;
  string expected_string;

  // default construction
  expected_string = "ro";
  EXPECT_EQ(expected_string, mount_options_.ToString());

  // options: ro (default)
  expected_string = "ro,nodev,noexec,nosuid";
  mount_options_.Initialize(options, false, "", "");
  EXPECT_EQ(expected_string, mount_options_.ToString());

  // options: ro, bind
  expected_string = "bind,ro,nodev,noexec,nosuid";
  options.push_back("bind");
  mount_options_.Initialize(options, false, "", "");
  EXPECT_EQ(expected_string, mount_options_.ToString());

  // options: ro, nodev
  expected_string = "ro,nodev,noexec,nosuid";
  options.clear();
  options.push_back("nodev");
  mount_options_.Initialize(options, false, "", "");
  EXPECT_EQ(expected_string, mount_options_.ToString());

  // options: nodev, rw
  expected_string = "rw,nodev,noexec,nosuid";
  options.push_back("rw");
  mount_options_.Initialize(options, false, "", "");
  EXPECT_EQ(expected_string, mount_options_.ToString());

  // options: nodev, rw, nosuid
  expected_string = "rw,nodev,noexec,nosuid";
  options.push_back("nosuid");
  mount_options_.Initialize(options, false, "", "");
  EXPECT_EQ(expected_string, mount_options_.ToString());

  // options: nodev, rw, nosuid, noexec
  expected_string = "rw,nodev,noexec,nosuid";
  options.push_back("noexec");
  mount_options_.Initialize(options, false, "", "");
  EXPECT_EQ(expected_string, mount_options_.ToString());

  // options: nodev, rw, nosuid, noexec, dirsync
  expected_string = "dirsync,rw,nodev,noexec,nosuid";
  options.push_back("dirsync");
  mount_options_.Initialize(options, false, "", "");
  EXPECT_EQ(expected_string, mount_options_.ToString());

  // options: nodev, rw, nosuid, noexec, dirsync, sync
  expected_string = "dirsync,sync,rw,nodev,noexec,nosuid";
  options.push_back("sync");
  mount_options_.Initialize(options, false, "", "");
  EXPECT_EQ(expected_string, mount_options_.ToString());

  // options: nodev, rw, nosuid, noexec, dirsync, sync
  // default uid=1000, gid=1001
  // ignore user and group ID
  expected_string = "dirsync,sync,rw,nodev,noexec,nosuid";
  mount_options_.Initialize(options, false, "1000", "1001");
  EXPECT_EQ(expected_string, mount_options_.ToString());

  // options: nodev, rw, nosuid, noexec, dirsync, sync
  // default uid=1000, gid=1001
  expected_string = "dirsync,sync,rw,uid=1000,gid=1001,nodev,noexec,nosuid";
  mount_options_.Initialize(options, true, "1000", "1001");
  EXPECT_EQ(expected_string, mount_options_.ToString());

  // options: nodev, rw, nosuid, noexec, dirsync, sync, uid=2000, gid=2001
  // default uid=1000, gid=1001
  // ignore user and group ID
  options.push_back("uid=2000");
  options.push_back("gid=2001");
  expected_string = "dirsync,sync,rw,nodev,noexec,nosuid";
  mount_options_.Initialize(options, false, "1000", "1001");
  EXPECT_EQ(expected_string, mount_options_.ToString());

  // options: nodev, rw, nosuid, noexec, dirsync, sync, uid=2000, gid=2001
  // default uid=1000, gid=1001
  expected_string = "dirsync,sync,rw,uid=2000,gid=2001,nodev,noexec,nosuid";
  mount_options_.Initialize(options, true, "1000", "1001");
  EXPECT_EQ(expected_string, mount_options_.ToString());

  // options: "nodev,dev"
  // ignore an option string containing a comma.
  expected_string = "ro,nodev,noexec,nosuid";
  options.clear();
  options.push_back("nodev,dev");
  mount_options_.Initialize(options, false, "", "");
  EXPECT_EQ(expected_string, mount_options_.ToString());
}

TEST_F(MountOptionsTest, ToMountFlagsAndData) {
  vector<string> options;
  MountOptions::Flags expected_flags;
  MountOptions::Flags security_flags = MS_NODEV | MS_NOEXEC | MS_NOSUID;
  string expected_data;
  pair<MountOptions::Flags, string> flags_and_data;

  // default construction
  expected_flags = MS_RDONLY;
  expected_data = "";
  flags_and_data = mount_options_.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: ro (default)
  mount_options_.Initialize(options, false, "", "");
  expected_flags = security_flags | MS_RDONLY;
  flags_and_data = mount_options_.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: ro, bind
  options.push_back("bind");
  expected_flags = security_flags | MS_RDONLY | MS_BIND;
  mount_options_.Initialize(options, false, "", "");
  flags_and_data = mount_options_.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: ro, nodev
  options.clear();
  options.push_back("nodev");
  expected_flags = security_flags | MS_RDONLY | MS_NODEV;
  mount_options_.Initialize(options, false, "", "");
  flags_and_data = mount_options_.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw
  options.push_back("rw");
  expected_flags = security_flags | MS_NODEV;
  mount_options_.Initialize(options, false, "", "");
  flags_and_data = mount_options_.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid
  options.push_back("nosuid");
  expected_flags = security_flags | MS_NODEV | MS_NOSUID;
  mount_options_.Initialize(options, false, "", "");
  flags_and_data = mount_options_.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid, noexec
  options.push_back("noexec");
  expected_flags = security_flags | MS_NODEV | MS_NOSUID | MS_NOEXEC;
  mount_options_.Initialize(options, false, "", "");
  flags_and_data = mount_options_.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid, noexec, sync
  options.push_back("sync");
  expected_flags = security_flags |
    MS_NODEV | MS_NOSUID | MS_NOEXEC | MS_SYNCHRONOUS;
  mount_options_.Initialize(options, false, "", "");
  flags_and_data = mount_options_.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid, noexec, sync
  // default uid=1000, gid=1001
  // ignore user and group ID
  expected_flags = security_flags | MS_NODEV | MS_NOSUID | MS_NOEXEC |
    MS_SYNCHRONOUS;
  mount_options_.Initialize(options, false, "1000", "1001");
  flags_and_data = mount_options_.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid, noexec, sync
  // default uid=1000, gid=1001
  expected_data = "uid=1000,gid=1001";
  mount_options_.Initialize(options, true, "1000", "1001");
  flags_and_data = mount_options_.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid, noexec, sync, uid=2000, gid=2001
  // default uid=1000, gid=1001
  // ignore user and group ID
  options.push_back("uid=2000");
  options.push_back("gid=2001");
  expected_data = "";
  mount_options_.Initialize(options, false, "1000", "1001");
  flags_and_data = mount_options_.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid, noexec, sync, uid=2000, gid=2001
  // default uid=1000, gid=1001
  expected_data = "uid=2000,gid=2001";
  mount_options_.Initialize(options, true, "1000", "1001");
  flags_and_data = mount_options_.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);
}

}  // namespace cros_disks
