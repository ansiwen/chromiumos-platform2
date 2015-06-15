// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_FILESYSTEM_INTERFACE_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_FILESYSTEM_INTERFACE_H_

// This class is used to abstract a filesystem and iterate the blocks
// associated with the files and filesystem structures.
// For the purposes of the update payload generation, a filesystem is a formated
// partition composed by fixed-size blocks, since that's the interface used in
// the update payload.

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>

#include "update_engine/update_metadata.pb.h"

namespace chromeos_update_engine {

class FilesystemInterface {
 public:
  // This represents a file or pseudo-file in the filesystem. It can include
  // all sort of files, like symlinks, hardlinks, directories and even a file
  // entry representing the metadata, free space, journaling data, etc.
  struct File {
    File() {
      memset(&file_stat, 0, sizeof(file_stat));
    }

    // The stat struct for the file. This is invalid (inode 0) for some
    // pseudo-files.
    struct stat file_stat;

    // The absolute path to the file inside the filesystem, for example,
    // "/usr/bin/bash". For pseudo-files, like blocks associated to internal
    // filesystem tables or free space, the path doesn't start with a /.
    std::string name;

    // The list of all physical blocks holding the data of this file in
    // the same order as the logical data. All the block numbers shall be
    // between 0 and GetBlockCount() - 1. The blocks are encoded in extents,
    // indicating the starting block, and the number of consecutive blocks.
    std::vector<Extent> extents;
  };

  virtual ~FilesystemInterface() = default;

  // Returns the size of a block in the filesystem.
  virtual size_t GetBlockSize() const = 0;

  // Returns the number of blocks in the filesystem.
  virtual size_t GetBlockCount() const = 0;

  // Stores in |files| the list of files and pseudo-files in the filesystem. See
  // FileInterface for details. The paths returned by this method shall not
  // be repeated; but the same block could be present in more than one file as
  // happens for example with hard-linked files, but not limited to those cases.
  // Returns whether the function succeeded.
  virtual bool GetFiles(std::vector<File>* files) const = 0;

 protected:
  FilesystemInterface() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FilesystemInterface);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_FILESYSTEM_INTERFACE_H_