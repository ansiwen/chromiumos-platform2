// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef METRICS_METRICS_LIBRARY_H_
#define METRICS_METRICS_LIBRARY_H_

#include <memory>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#include <base/compiler_specific.h>
#include <base/macros.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "policy/libpolicy.h"

class MetricsLibraryInterface {
 public:
  virtual void Init() = 0;
  virtual bool AreMetricsEnabled() = 0;
  virtual bool SendToUMA(const std::string& name, int sample,
                         int min, int max, int nbuckets) = 0;
  virtual bool SendEnumToUMA(const std::string& name, int sample, int max) = 0;
  virtual bool SendBoolToUMA(const std::string& name, bool sample) = 0;
  virtual bool SendSparseToUMA(const std::string& name, int sample) = 0;
  virtual bool SendUserActionToUMA(const std::string& action) = 0;
  virtual ~MetricsLibraryInterface() {}
};

// Library used to send metrics to Chrome/UMA.
class MetricsLibrary : public MetricsLibraryInterface {
 public:
  MetricsLibrary();
  virtual ~MetricsLibrary();

  // Initializes the library.
  void Init() override;

  // Returns whether or not the machine is running in guest mode.
  bool IsGuestMode();

  // Returns whether or not metrics collection is enabled.
  bool AreMetricsEnabled() override;

  // Chrome normally manages Enable/Disable state. These functions are
  // intended ONLY for use by devices which don't run Chrome (e.g. Onhub)
  // but are based on Chrome OS.
  // In those cases, "User Consent" is given via an "external" app
  // (e.g. cloud service or directly from a smart phone app).
  //
  // Enable metrics by creating and populating the Consent file.
  bool EnableMetrics();

  // Disable metrics by deleting the Consent file.
  bool DisableMetrics();

  // Look up the consent id for metrics reporting.
  // Note: Should only be used by internal system projects.
  bool ConsentId(std::string* id);

  // Sends histogram data to Chrome for transport to UMA and returns
  // true on success. This method results in the equivalent of an
  // asynchronous non-blocking RPC to UMA_HISTOGRAM_CUSTOM_COUNTS
  // inside Chrome (see base/histogram.h).
  //
  // |sample| is the sample value to be recorded (|min| <= |sample| < |max|).
  // |min| is the minimum value of the histogram samples (|min| > 0).
  // |max| is the maximum value of the histogram samples.
  // |nbuckets| is the number of histogram buckets.
  // [0,min) is the implicit underflow bucket.
  // [|max|,infinity) is the implicit overflow bucket.
  //
  // Note that the memory allocated in Chrome for each histogram is
  // proportional to the number of buckets. Therefore, it is strongly
  // recommended to keep this number low (e.g., 50 is normal, while
  // 100 is high).
  bool SendToUMA(const std::string& name, int sample,
                 int min, int max, int nbuckets) override;

  // Sends linear histogram data to Chrome for transport to UMA and
  // returns true on success. This method results in the equivalent of
  // an asynchronous non-blocking RPC to UMA_HISTOGRAM_ENUMERATION
  // inside Chrome (see base/histogram.h).
  //
  // |sample| is the sample value to be recorded (1 <= |sample| < |max|).
  // |max| is the maximum value of the histogram samples.
  // 0 is the implicit underflow bucket.
  // [|max|,infinity) is the implicit overflow bucket.
  //
  // An enumeration histogram requires |max| + 1 number of
  // buckets. Note that the memory allocated in Chrome for each
  // histogram is proportional to the number of buckets. Therefore, it
  // is strongly recommended to keep this number low (e.g., 50 is
  // normal, while 100 is high).
  bool SendEnumToUMA(const std::string& name, int sample, int max) override;

  // Specialization of SendEnumToUMA for boolean values.
  bool SendBoolToUMA(const std::string& name, bool sample) override;

  // Sends sparse histogram sample to Chrome for transport to UMA.  Returns
  // true on success.
  //
  // |sample| is the 32-bit integer value to be recorded.
  bool SendSparseToUMA(const std::string& name, int sample) override;

  // Sends a user action to Chrome for transport to UMA and returns true on
  // success. This method results in the equivalent of an asynchronous
  // non-blocking RPC to UserMetrics::RecordAction.  The new metric must be
  // added to chrome/tools/extract_actions.py in the Chromium repository, which
  // should then be run to generate a hash for the new action.
  //
  // Until http://crosbug.com/11125 is fixed, the metric must also be added to
  // chrome/browser/chromeos/external_metrics.cc.
  //
  // |action| is the user-generated event (e.g., "MuteKeyPressed").
  bool SendUserActionToUMA(const std::string& action) override;

  // Sends a signal to UMA that a crash of the given |crash_kind|
  // has occurred.  Used by UMA to generate stability statistics.
  bool SendCrashToUMA(const char *crash_kind);

  // Sends a "generic Chrome OS event" to UMA.  This is an event name
  // that is translated into an enumerated histogram entry.  Event names
  // must first be registered in metrics_library.cc.  See that file for
  // more details.
  bool SendCrosEventToUMA(const std::string& event);

 private:
  friend class CMetricsLibraryTest;
  friend class MetricsLibraryTest;
  FRIEND_TEST(MetricsLibraryTest, AreMetricsEnabled);
  FRIEND_TEST(MetricsLibraryTest, FormatChromeMessage);
  FRIEND_TEST(MetricsLibraryTest, FormatChromeMessageTooLong);
  FRIEND_TEST(MetricsLibraryTest, IsDeviceMounted);
  FRIEND_TEST(MetricsLibraryTest, SendMessageToChrome);
  FRIEND_TEST(MetricsLibraryTest, SendMessageToChromeUMAEventsBadFileLocation);

  // Sets |*result| to whether or not the |mounts_file| indicates that
  // the |device_name| is currently mounted.  Uses |buffer| of
  // |buffer_size| to read the file.  Returns false if any error.
  bool IsDeviceMounted(const char* device_name,
                       const char* mounts_file,
                       char* buffer, int buffer_size,
                       bool* result);

  // This function is used by tests only to mock the device policies.
  void SetPolicyProvider(policy::PolicyProvider* provider);

  // Time at which we last checked if metrics were enabled.
  static time_t cached_enabled_time_;

  // Cached state of whether or not metrics were enabled.
  static bool cached_enabled_;

  std::string uma_events_file_;
  std::string consent_file_;

  std::unique_ptr<policy::PolicyProvider> policy_provider_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLibrary);
};

#endif  // METRICS_METRICS_LIBRARY_H_
