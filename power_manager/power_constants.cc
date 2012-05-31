// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/power_constants.h"

namespace power_manager {

const char kPluggedBrightnessOffsetPref[] = "plugged_brightness_offset";
const char kUnpluggedBrightnessOffsetPref[] = "unplugged_brightness_offset";
const char kLowBatterySuspendPercentPref[] = "low_battery_suspend_percent";
const char kCleanShutdownTimeoutMsPref[] = "clean_shutdown_timeout_ms";
const char kPluggedDimMsPref[] = "plugged_dim_ms";
const char kPluggedOffMsPref[] = "plugged_off_ms";
const char kUnpluggedDimMsPref[] = "unplugged_dim_ms";
const char kUnpluggedOffMsPref[] = "unplugged_off_ms";
const char kUnpluggedSuspendMsPref[] = "unplugged_suspend_ms";
const char kEnforceLockPref[] = "enforce_lock";
const char kDisableIdleSuspendPref[] = "disable_idle_suspend";
const char kUseLidPref[] = "use_lid";
const char kLockOnIdleSuspendPref[] = "lock_on_idle_suspend";
const char kLockMsPref[] = "lock_ms";
const char kRetrySuspendMsPref[] = "retry_suspend_ms";
const char kRetrySuspendAttemptsPref[] = "retry_suspend_attempts";
const char kPluggedSuspendMsPref[] = "plugged_suspend_ms";
const char kUseXScreenSaverPref[] = "use_xscreensaver";
const char kMinVisibleBacklightLevelPref[] = "min_visible_backlight_level";
const char kDisableALSPref[] = "disable_als";
const char kWakeupInputPref[] = "wakeup_input_device_names";
// The minimum delta between timers when we want to give a user time to react.
const char kReactMsPref[] = "react_ms";
// The minimum delta between timers to avoid timer precision issues.
const char kFuzzMsPref[] = "fuzz_ms";
// The maximum duration in seconds the state machine can be disabled for
const char kStateMaxDisabledDurationSecPref[] =
    "state_max_disabled_duration_sec";

const char kBacklightPath[] = "/sys/class/backlight";
const char kBacklightPattern[] = "*";
const char kKeyboardBacklightPath[] = "/sys/class/leds";
const char kKeyboardBacklightPattern[] = "*:kbd_backlight";

// Interface names.
const char kRootPowerManagerInterface[] = "org.chromium.RootPowerManager";
const char kRootPowerManagerServiceName[] = "org.chromium.RootPowerManager";

// powerd -> powerm signals.
const char kCheckLidStateSignal[] = "CheckLidStateSignal";
const char kRestartSignal[] = "RestartSignal";
const char kRequestCleanShutdown[] = "RequestCleanShutdown";
const char kSuspendSignal[] = "SuspendSignal";
const char kShutdownSignal[] = "ShutdownSignal";
const char kExternalBacklightGetMethod[] = "ExternalBacklightGet";
const char kExternalBacklightSetMethod[] = "ExternalBacklightSet";

// powerm -> powerd signals.
const char kLidClosed[] = "LidClosed";
const char kLidOpened[] = "LidOpened";
const char kExternalBacklightUpdate[] = "ExternalBacklightUpdate";
const char kKeyLeftCtrl[] = "LeftCtrlKey";
const char kKeyRightCtrl[] = "RightCtrlKey";
const char kKeyLeftAlt[] = "LeftAltKey";
const char kKeyRightAlt[] = "RightAltKey";
const char kKeyLeftShift[] = "LeftShiftKey";
const char kKeyRightShift[] = "RightShiftKey";
const char kKeyF4[] = "F4Key";

// Broadcast signals.
const char kPowerStateChanged[] = "PowerStateChanged";

// Files to signal powerd_suspend whether suspend should be cancelled.
const char kLidOpenFile[] = "lid_opened";
const char kUserActiveFile[] = "user_active";

}  // namespace power_manager
