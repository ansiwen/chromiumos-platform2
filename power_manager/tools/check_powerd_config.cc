// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <cstdlib>

#include <base/at_exit.h>
#include <base/format_macros.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <brillo/flag_helper.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"

int main(int argc, char* argv[]) {
  DEFINE_bool(ambient_light_sensor,
              false,
              "Exit with success if ambient light sensor support is enabled");
  DEFINE_bool(hover_detection,
              false,
              "Exit with success if hover detection is enabled");
  DEFINE_bool(keyboard_backlight,
              false,
              "Exit with success if keyboard backlight support is enabled");
  DEFINE_bool(low_battery_shutdown_percent,
              false,
              "Print the percent-based low-battery shutdown threshold (in "
              "[0.0, 100.0]) to stdout");
  DEFINE_bool(low_battery_shutdown_time,
              false,
              "Print the time-based low-battery shutdown threshold (in "
              "seconds) to stdout");
  DEFINE_bool(suspend_to_idle,
              false,
              "Exit with success if \"freeze\" (rather than \"mem\") will be "
              "written to /sys/power/state when suspending");

  brillo::FlagHelper::Init(
      argc, argv, "Check the device's power-related configuration");

  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;

  CHECK_EQ((FLAGS_ambient_light_sensor ? 1 : 0) +
               (FLAGS_hover_detection ? 1 : 0) +
               (FLAGS_keyboard_backlight ? 1 : 0) +
               (FLAGS_low_battery_shutdown_percent ? 1 : 0) +
               (FLAGS_low_battery_shutdown_time ? 1 : 0) +
               (FLAGS_suspend_to_idle ? 1 : 0),
           1)
      << "Exactly one flag must be set";

  power_manager::Prefs prefs;
  CHECK(prefs.Init(power_manager::util::GetPrefPaths(
      base::FilePath(power_manager::kReadWritePrefsDir),
      base::FilePath(power_manager::kReadOnlyPrefsDir))));

  if (FLAGS_ambient_light_sensor) {
    bool als_enabled = false;
    prefs.GetBool(power_manager::kHasAmbientLightSensorPref, &als_enabled);
    exit(als_enabled ? 0 : 1);
  } else if (FLAGS_hover_detection) {
    bool hover_enabled = false;
    prefs.GetBool(power_manager::kDetectHoverPref, &hover_enabled);
    exit(hover_enabled ? 0 : 1);
  } else if (FLAGS_keyboard_backlight) {
    bool backlight_enabled = false;
    prefs.GetBool(power_manager::kHasKeyboardBacklightPref, &backlight_enabled);
    exit(backlight_enabled ? 0 : 1);
  } else if (FLAGS_low_battery_shutdown_percent) {
    double percent = 0.0;
    prefs.GetDouble(power_manager::kLowBatteryShutdownPercentPref, &percent);
    printf("%.1f\n", percent);
    exit(0);
  } else if (FLAGS_low_battery_shutdown_time) {
    int64_t sec = 0;
    double p = 0.0;
    // Match system::PowerSupply's logic: a time-based threshold is ignored if a
    // percent-based threshold is set.
    if (!prefs.GetDouble(power_manager::kLowBatteryShutdownPercentPref, &p))
      prefs.GetInt64(power_manager::kLowBatteryShutdownTimePref, &sec);
    printf("%" PRId64 "\n", sec);
    exit(0);
  } else if (FLAGS_suspend_to_idle) {
    bool suspend_to_idle = false;
    prefs.GetBool(power_manager::kSuspendToIdlePref, &suspend_to_idle);
    exit(suspend_to_idle ? 0 : 1);
  } else {
    NOTREACHED();
    exit(1);
  }
}
