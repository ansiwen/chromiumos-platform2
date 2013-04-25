// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_ISOLATE_MOCK_H_
#define CHAPS_ISOLATE_MOCK_H_

#include "chaps/isolate.h"

#include <string>

#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace chaps {

class IsolateCredentialManagerMock : public IsolateCredentialManager {
 public:
  MOCK_METHOD1(GetCurrentUserIsolateCredential, bool (chromeos::SecureBlob*));
  MOCK_METHOD2(GetUserIsolateCredential, bool (const std::string&,
                                               chromeos::SecureBlob*));
  MOCK_METHOD2(SaveIsolateCredential, bool (const std::string&,
                                            const chromeos::SecureBlob&));
};

}  // namespace chaps

#endif  // CHAPS_ISOLATE_MOCK_H_
