// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mist/metrics.h"

#include <base/logging.h>

namespace mist {

namespace {

enum SwitchResult {
  kSwitchResultSuccess,
  kSwitchResultFailure,
  kSwitchResultMaxValue
};

}  // namespace

Metrics::Metrics() {
  metrics_library_.Init();
}

void Metrics::RecordSwitchResult(bool success) {
  if (!metrics_library_.SendEnumToUMA(
          "Mist.SwitchResult",
          success ? kSwitchResultSuccess : kSwitchResultFailure,
          kSwitchResultMaxValue))
    LOG(WARNING) << "Could not send switch result sample to UMA.";
}

}  // namespace mist
