// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "midis/device_tracker.h"

#include <utility>

#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <brillo/test_helpers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "midis/test_helper.h"

using ::testing::_;
using ::testing::Return;

namespace midis {

namespace {

const char kFakeName1[] = "Sample MIDI Device - 1";
const uint32_t kFakeSysNum1 = 2;
const uint32_t kFakeDevNum1 = 0;
const uint32_t kFakeSubdevs1 = 1;
const uint32_t kFakeFlags1 = 7;

const char kFakeName2[] = "Sample MIDI Device - 2";
const uint32_t kFakeSysNum2 = 3;
const uint32_t kFakeDevNum2 = 1;
const uint32_t kFakeSubdevs2 = 2;
const uint32_t kFakeFlags2 = 6;

}  // namespace

class DeviceTrackerTest : public ::testing::Test {
 protected:
  DeviceTracker tracker_;
};

// Check whether a 2 devices get successfully added to the devices map.
TEST_F(DeviceTrackerTest, Add2DevicesPositive) {
  tracker_.AddDevice(base::MakeUnique<Device>(
      kFakeName1, kFakeSysNum1, kFakeDevNum1, kFakeSubdevs1, kFakeFlags1));

  tracker_.AddDevice(base::MakeUnique<Device>(
      kFakeName2, kFakeSysNum2, kFakeDevNum2, kFakeSubdevs2, kFakeFlags2));

  EXPECT_EQ(2, tracker_.devices_.size());

  auto it = tracker_.devices_.begin();
  uint32_t device_id = it->first;
  Device const* device = it->second.get();
  EXPECT_THAT(device, DeviceMatcher(device_id, kFakeName1));

  it++;
  device_id = it->first;
  device = it->second.get();
  EXPECT_THAT(device, DeviceMatcher(device_id, kFakeName2));
}

// Check whether a device gets successfully added, then removed from the devices
// map.
TEST_F(DeviceTrackerTest, AddRemoveDevicePositive) {
  tracker_.AddDevice(base::MakeUnique<Device>(
      kFakeName1, kFakeSysNum1, kFakeDevNum1, kFakeSubdevs1, kFakeFlags1));
  EXPECT_EQ(1, tracker_.devices_.size());

  tracker_.RemoveDevice(kFakeSysNum1, kFakeDevNum1);
  EXPECT_EQ(0, tracker_.devices_.size());
}

// Check whether a device gets successfully added, but not removed.
TEST_F(DeviceTrackerTest, AddDeviceRemoveNegative) {
  tracker_.AddDevice(base::MakeUnique<Device>(
      kFakeName1, kFakeSysNum1, kFakeDevNum1, kFakeSubdevs1, kFakeFlags1));
  EXPECT_EQ(1, tracker_.devices_.size());

  tracker_.RemoveDevice(kFakeSysNum2, kFakeDevNum1);
  EXPECT_EQ(1, tracker_.devices_.size());
}

}  // namespace midis

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}
