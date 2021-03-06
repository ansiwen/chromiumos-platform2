// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/mock_context.h"

#include "mist/event_dispatcher.h"
#include "mist/mock_config_loader.h"
#include "mist/mock_udev.h"
#include "mist/usb_device_event_notifier.h"

namespace mist {

bool MockContext::Initialize() {
  config_loader_.reset(new MockConfigLoader());
  CHECK(config_loader_);

  event_dispatcher_.reset(new EventDispatcher());
  CHECK(event_dispatcher_);

  udev_.reset(new MockUdev());
  CHECK(udev_);

  usb_device_event_notifier_.reset(
      new UsbDeviceEventNotifier(event_dispatcher_.get(), udev_.get()));
  CHECK(usb_device_event_notifier_);

  // TODO(benchan): Initialize |usb_manager_| with a MockUsbManager object.
  return true;
}

MockConfigLoader* MockContext::GetMockConfigLoader() const {
  return static_cast<MockConfigLoader*>(config_loader_.get());
}

MockUdev* MockContext::GetMockUdev() const {
  return static_cast<MockUdev*>(udev_.get());
}

}  // namespace mist
