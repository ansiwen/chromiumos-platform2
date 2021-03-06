// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_MOUNT_ENTRY_H_
#define CROS_DISKS_MOUNT_ENTRY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include <chromeos/dbus/service_constants.h>
#include <dbus-c++/dbus.h>

namespace cros_disks {

using DBusMountEntry =
    ::DBus::Struct<uint32_t, std::string, uint32_t, std::string>;
using DBusMountEntries = std::vector<DBusMountEntry>;

class MountEntry {
 public:
  MountEntry(MountErrorType error_type,
             const std::string& source_path,
             MountSourceType source_type,
             const std::string& mount_path,
             bool is_read_only);
  ~MountEntry() = default;

  DBusMountEntry ToDBusFormat() const;

  MountErrorType error_type() const { return error_type_; }
  const std::string& source_path() const { return source_path_; }
  MountSourceType source_type() const { return source_type_; }
  const std::string& mount_path() const { return mount_path_; }
  bool is_read_only() const { return is_read_only_; }

 private:
  MountErrorType error_type_;
  std::string source_path_;
  MountSourceType source_type_;
  std::string mount_path_;
  bool is_read_only_;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_MOUNT_ENTRY_H_
