// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_IPCONFIG_
#define SHILL_MOCK_IPCONFIG_

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/ipconfig.h"

namespace shill {

class MockIPConfig : public IPConfig {
 public:
  MockIPConfig(ControlInterface *control_interface,
               const std::string &device_name);
  virtual ~MockIPConfig();

  MOCK_CONST_METHOD0(properties, const Properties &(void));
  MOCK_METHOD0(RequestIP, bool(void));
  MOCK_METHOD0(RenewIP, bool(void));
  MOCK_METHOD0(ReleaseIP, bool(void));

  MOCK_METHOD2(Load, bool(StoreInterface *storage,
                          const std::string &id_suffix));
  MOCK_METHOD2(Save, bool(StoreInterface *storage,
                          const std::string &id_suffix));
  MOCK_METHOD0(EmitChanges, void(void));

 private:
  const Properties &real_properties() {
    return IPConfig::properties();
  }

  DISALLOW_COPY_AND_ASSIGN(MockIPConfig);
};

}  // namespace shill

#endif  // SHILL_MOCK_IPCONFIG_
