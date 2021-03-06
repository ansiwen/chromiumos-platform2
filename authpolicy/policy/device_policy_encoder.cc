// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/policy/device_policy_encoder.h"

#include <memory>
#include <utility>

#include <base/json/json_reader.h>
#include <base/strings/string_number_conversions.h>
#include <components/policy/core/common/registry_dict.h>
#include <dbus/shill/dbus-constants.h>

#include "authpolicy/log_level.h"
#include "authpolicy/policy/policy_encoder_helper.h"
#include "bindings/chrome_device_policy.pb.h"
#include "bindings/policy_constants.h"

namespace em = enterprise_management;

namespace policy {

constexpr std::pair<const char*, int> kConnectionTypes[] = {
    std::make_pair(
        shill::kTypeEthernet,
        em::AutoUpdateSettingsProto_ConnectionType_CONNECTION_TYPE_ETHERNET),
    std::make_pair(
        shill::kTypeWifi,
        em::AutoUpdateSettingsProto_ConnectionType_CONNECTION_TYPE_WIFI),
    std::make_pair(
        shill::kTypeWimax,
        em::AutoUpdateSettingsProto_ConnectionType_CONNECTION_TYPE_WIMAX),
    std::make_pair(
        shill::kTypeBluetooth,
        em::AutoUpdateSettingsProto_ConnectionType_CONNECTION_TYPE_BLUETOOTH),
    std::make_pair(
        shill::kTypeCellular,
        em::AutoUpdateSettingsProto_ConnectionType_CONNECTION_TYPE_CELLULAR)};

constexpr size_t kConnectionTypesSize = arraysize(kConnectionTypes);

static_assert(
    em::AutoUpdateSettingsProto_ConnectionType_ConnectionType_ARRAYSIZE ==
        static_cast<int>(kConnectionTypesSize),
    "Add all values here");

namespace {

// Translates string connection types to enums.
bool DecodeConnectionType(const std::string& value,
                          em::AutoUpdateSettingsProto_ConnectionType* type) {
  DCHECK(type);

  for (size_t n = 0; n < arraysize(kConnectionTypes); ++n) {
    if (value.compare(kConnectionTypes[n].first) == 0) {
      int int_type = kConnectionTypes[n].second;
      DCHECK(em::AutoUpdateSettingsProto_ConnectionType_IsValid(int_type));
      *type = static_cast<em::AutoUpdateSettingsProto_ConnectionType>(int_type);
      return true;
    }
  }

  LOG(ERROR) << "Invalid connection type '" << value << "'.";
  return false;
}

// Parses the |json| string to a base::DictionaryValue. Returns nullptr on error
// and sets the |error| string.
std::unique_ptr<base::DictionaryValue> JsonToDictionary(const std::string& json,
                                                        std::string* error) {
  DCHECK(error);
  std::unique_ptr<base::Value> root = base::JSONReader::ReadAndReturnError(
      json, base::JSON_ALLOW_TRAILING_COMMAS, NULL, error);
  if (!root)
    return nullptr;  // |error| is set by ReadAndReturnError().

  std::unique_ptr<base::DictionaryValue> dict_value =
      base::DictionaryValue::From(std::move(root));
  if (!dict_value)
    *error = "Json is not a dictionary: '" + json + "'";
  return dict_value;
}

}  //  namespace

void DevicePolicyEncoder::EncodePolicy(
    em::ChromeDeviceSettingsProto* policy) const {
  EncodeLoginPolicies(policy);
  EncodeNetworkPolicies(policy);
  EncodeReportingPolicies(policy);
  EncodeAutoUpdatePolicies(policy);
  EncodeAccessibilityPolicies(policy);
  EncodeGenericPolicies(policy);
}

void DevicePolicyEncoder::EncodeLoginPolicies(
    em::ChromeDeviceSettingsProto* policy) const {
  EncodeBoolean(key::kDeviceGuestModeEnabled, [policy](bool value) {
    policy->mutable_guest_mode_enabled()->set_guest_mode_enabled(value);
  });
  EncodeBoolean(key::kDeviceRebootOnShutdown, [policy](bool value) {
    policy->mutable_reboot_on_shutdown()->set_reboot_on_shutdown(value);
  });
  EncodeBoolean(key::kDeviceShowUserNamesOnSignin, [policy](bool value) {
    policy->mutable_show_user_names()->set_show_user_names(value);
  });
  EncodeBoolean(key::kDeviceAllowNewUsers, [policy](bool value) {
    policy->mutable_allow_new_users()->set_allow_new_users(value);
  });
  EncodeStringList(key::kDeviceUserWhitelist,
                   [policy](const std::vector<std::string>& values) {
                     auto list = policy->mutable_user_whitelist();
                     for (const std::string& value : values)
                       list->add_user_whitelist(value);
                   });
  EncodeBoolean(key::kDeviceEphemeralUsersEnabled, [policy](bool value) {
    policy->mutable_ephemeral_users_enabled()->set_ephemeral_users_enabled(
        value);
  });

  // TODO(ljusten): It is not clear how to map the DeviceLocalAccounts policy
  // to DeviceLocalAccountsProto accounts. The former is a string with no json
  // schema, the latter is a struct with id, type and kiosk info.
  HandleUnsupported(key::kDeviceLocalAccounts);
  EncodeString(
      key::kDeviceLocalAccountAutoLoginId, [policy](const std::string& value) {
        policy->mutable_device_local_accounts()->set_auto_login_id(value);
      });
  EncodeInteger(key::kDeviceLocalAccountAutoLoginDelay, [policy](int value) {
    policy->mutable_device_local_accounts()->set_auto_login_delay(value);
  });
  EncodeBoolean(
      key::kDeviceLocalAccountAutoLoginBailoutEnabled, [policy](bool value) {
        policy->mutable_device_local_accounts()->set_enable_auto_login_bailout(
            value);
      });
  EncodeBoolean(key::kDeviceLocalAccountPromptForNetworkWhenOffline,
                [policy](bool value) {
                  policy->mutable_device_local_accounts()
                      ->set_prompt_for_network_when_offline(value);
                });

  EncodeBoolean(key::kSupervisedUsersEnabled, [policy](bool value) {
    policy->mutable_supervised_users_settings()->set_supervised_users_enabled(
        value);
  });
  EncodeBoolean(key::kDeviceTransferSAMLCookies, [policy](bool value) {
    policy->mutable_saml_settings()->set_transfer_saml_cookies(value);
  });
  EncodeInteger(key::kLoginAuthenticationBehavior, [policy](int value) {
    policy->mutable_login_authentication_behavior()
        ->set_login_authentication_behavior(
            static_cast<em::LoginAuthenticationBehaviorProto_LoginBehavior>(
                value));
  });
  EncodeBoolean(key::kDeviceAllowBluetooth, [policy](bool value) {
    policy->mutable_allow_bluetooth()->set_allow_bluetooth(value);
  });
  EncodeStringList(key::kLoginVideoCaptureAllowedUrls,
                   [policy](const std::vector<std::string>& values) {
                     auto list =
                         policy->mutable_login_video_capture_allowed_urls();
                     for (const std::string& value : values)
                       list->add_urls(value);
                   });
  EncodeStringList(key::kDeviceLoginScreenAppInstallList,
                   [policy](const std::vector<std::string>& values) {
                     auto list =
                         policy->mutable_device_login_screen_app_install_list();
                     for (const std::string& value : values)
                       list->add_device_login_screen_app_install_list(value);
                   });
  EncodeStringList(key::kDeviceLoginScreenLocales,
                   [policy](const std::vector<std::string>& values) {
                     auto list = policy->mutable_login_screen_locales();
                     for (const std::string& value : values)
                       list->add_login_screen_locales(value);
                   });
  EncodeStringList(key::kDeviceLoginScreenInputMethods,
                   [policy](const std::vector<std::string>& values) {
                     auto list = policy->mutable_login_screen_input_methods();
                     for (const std::string& value : values)
                       list->add_login_screen_input_methods(value);
                   });
}

void DevicePolicyEncoder::EncodeNetworkPolicies(
    em::ChromeDeviceSettingsProto* policy) const {
  EncodeBoolean(key::kDeviceDataRoamingEnabled, [policy](bool value) {
    policy->mutable_data_roaming_enabled()->set_data_roaming_enabled(value);
  });

  EncodeString(
      key::kNetworkThrottlingEnabled, [policy](const std::string& value) {
        std::string error;
        std::unique_ptr<base::DictionaryValue> dict_value =
            JsonToDictionary(value, &error);
        bool enabled;
        int upload_rate_kbits, download_rate_kbits;
        if (!dict_value || !dict_value->GetBoolean("enabled", &enabled) ||
            !dict_value->GetInteger("upload_rate_kbits", &upload_rate_kbits) ||
            !dict_value->GetInteger("download_rate_kbits",
                                    &download_rate_kbits)) {
          LOG(ERROR) << "Invalid JSON string '"
                     << (!error.empty() ? error : value) << "' for policy '"
                     << key::kNetworkThrottlingEnabled
                     << "', ignoring. Expected: "
                     << "'{\"enabled\"=<true/false>, \"upload_rate_kbits\""
                     << "=<kbits>, \"download_rate_kbits\"=<kbits>}'.";
          return;
        }
        em::NetworkThrottlingEnabledProto* entry =
            policy->mutable_network_throttling();
        entry->set_enabled(enabled);
        entry->set_upload_rate_kbits(upload_rate_kbits);
        entry->set_download_rate_kbits(download_rate_kbits);
      });

  EncodeString(key::kDeviceOpenNetworkConfiguration,
               [policy](const std::string& value) {
                 policy->mutable_open_network_configuration()
                     ->set_open_network_configuration(value);
               });
}

void DevicePolicyEncoder::EncodeReportingPolicies(
    em::ChromeDeviceSettingsProto* policy) const {
  EncodeBoolean(key::kReportDeviceVersionInfo, [policy](bool value) {
    policy->mutable_device_reporting()->set_report_version_info(value);
  });
  EncodeBoolean(key::kReportDeviceActivityTimes, [policy](bool value) {
    policy->mutable_device_reporting()->set_report_activity_times(value);
  });
  EncodeBoolean(key::kReportDeviceBootMode, [policy](bool value) {
    policy->mutable_device_reporting()->set_report_boot_mode(value);
  });
  EncodeBoolean(key::kReportDeviceLocation, [policy](bool value) {
    policy->mutable_device_reporting()->set_report_location(value);
  });
  EncodeBoolean(key::kReportDeviceNetworkInterfaces, [policy](bool value) {
    policy->mutable_device_reporting()->set_report_network_interfaces(value);
  });
  EncodeBoolean(key::kReportDeviceUsers, [policy](bool value) {
    policy->mutable_device_reporting()->set_report_users(value);
  });
  EncodeBoolean(key::kReportDeviceHardwareStatus, [policy](bool value) {
    policy->mutable_device_reporting()->set_report_hardware_status(value);
  });
  EncodeBoolean(key::kReportDeviceSessionStatus, [policy](bool value) {
    policy->mutable_device_reporting()->set_report_session_status(value);
  });
  EncodeBoolean(key::kReportUploadFrequency, [policy](bool value) {
    policy->mutable_device_reporting()->set_device_status_frequency(value);
  });

  EncodeBoolean(key::kHeartbeatEnabled, [policy](bool value) {
    policy->mutable_device_heartbeat_settings()->set_heartbeat_enabled(value);
  });
  EncodeInteger(key::kHeartbeatFrequency, [policy](int value) {
    policy->mutable_device_heartbeat_settings()->set_heartbeat_frequency(value);
  });

  EncodeBoolean(key::kLogUploadEnabled, [policy](bool value) {
    policy->mutable_device_log_upload_settings()->set_system_log_upload_enabled(
        value);
  });
}

void DevicePolicyEncoder::EncodeAutoUpdatePolicies(
    em::ChromeDeviceSettingsProto* policy) const {
  EncodeString(key::kChromeOsReleaseChannel,
               [policy](const std::string& value) {
                 policy->mutable_release_channel()->set_release_channel(value);
               });
  EncodeBoolean(key::kChromeOsReleaseChannelDelegated, [policy](bool value) {
    policy->mutable_release_channel()->set_release_channel_delegated(value);
  });

  EncodeBoolean(key::kDeviceAutoUpdateDisabled, [policy](bool value) {
    policy->mutable_auto_update_settings()->set_update_disabled(value);
  });
  EncodeString(
      key::kDeviceTargetVersionPrefix, [policy](const std::string& value) {
        policy->mutable_auto_update_settings()->set_target_version_prefix(
            value);
      });
  // target_version_display_name is not actually a policy, but a display
  // string for target_version_prefix, so we ignore it. It seems to be
  // unreferenced as well.
  EncodeInteger(key::kDeviceUpdateScatterFactor, [policy](int value) {
    policy->mutable_auto_update_settings()->set_scatter_factor_in_seconds(
        value);
  });
  EncodeStringList(key::kDeviceUpdateAllowedConnectionTypes,
                   [policy](const std::vector<std::string>& values) {
                     auto list = policy->mutable_auto_update_settings();
                     for (const std::string& value : values) {
                       em::AutoUpdateSettingsProto_ConnectionType type;
                       if (DecodeConnectionType(value, &type))
                         list->add_allowed_connection_types(type);
                     }
                   });
  EncodeBoolean(key::kDeviceUpdateHttpDownloadsEnabled, [policy](bool value) {
    policy->mutable_auto_update_settings()->set_http_downloads_enabled(value);
  });
  EncodeBoolean(key::kRebootAfterUpdate, [policy](bool value) {
    policy->mutable_auto_update_settings()->set_reboot_after_update(value);
  });
  EncodeBoolean(key::kDeviceAutoUpdateP2PEnabled, [policy](bool value) {
    policy->mutable_auto_update_settings()->set_p2p_enabled(value);
  });

  EncodeBoolean(key::kAllowKioskAppControlChromeVersion, [policy](bool value) {
    policy->mutable_allow_kiosk_app_control_chrome_version()
        ->set_allow_kiosk_app_control_chrome_version(value);
  });
}

void DevicePolicyEncoder::EncodeAccessibilityPolicies(
    em::ChromeDeviceSettingsProto* policy) const {
  EncodeBoolean(key::kDeviceLoginScreenDefaultLargeCursorEnabled,
                [policy](bool value) {
                  policy->mutable_accessibility_settings()
                      ->set_login_screen_default_large_cursor_enabled(value);
                });
  EncodeBoolean(key::kDeviceLoginScreenDefaultSpokenFeedbackEnabled,
                [policy](bool value) {
                  policy->mutable_accessibility_settings()
                      ->set_login_screen_default_spoken_feedback_enabled(value);
                });
  EncodeBoolean(key::kDeviceLoginScreenDefaultHighContrastEnabled,
                [policy](bool value) {
                  policy->mutable_accessibility_settings()
                      ->set_login_screen_default_high_contrast_enabled(value);
                });
  EncodeInteger(
      key::kDeviceLoginScreenDefaultScreenMagnifierType, [policy](int value) {
        policy->mutable_accessibility_settings()
            ->set_login_screen_default_screen_magnifier_type(
                static_cast<em::AccessibilitySettingsProto_ScreenMagnifierType>(
                    value));
      });
  EncodeBoolean(
      key::kDeviceLoginScreenDefaultVirtualKeyboardEnabled,
      [policy](bool value) {
        policy->mutable_accessibility_settings()
            ->set_login_screen_default_virtual_keyboard_enabled(value);
      });
}

void DevicePolicyEncoder::EncodeGenericPolicies(
    em::ChromeDeviceSettingsProto* policy) const {
  EncodeInteger(key::kDevicePolicyRefreshRate, [policy](int value) {
    policy->mutable_device_policy_refresh_rate()
        ->set_device_policy_refresh_rate(value);
  });

  EncodeBoolean(key::kDeviceMetricsReportingEnabled, [policy](bool value) {
    policy->mutable_metrics_enabled()->set_metrics_enabled(value);
  });

  EncodeString(key::kSystemTimezone, [policy](const std::string& value) {
    policy->mutable_system_timezone()->set_timezone(value);
  });
  EncodeInteger(key::kSystemTimezoneAutomaticDetection, [policy](int value) {
    policy->mutable_system_timezone()->set_timezone_detection_type(
        static_cast<em::SystemTimezoneProto_AutomaticTimezoneDetectionType>(
            value));
  });
  EncodeBoolean(key::kSystemUse24HourClock, [policy](bool value) {
    policy->mutable_use_24hour_clock()->set_use_24hour_clock(value);
  });

  EncodeBoolean(
      key::kDeviceAllowRedeemChromeOsRegistrationOffers, [policy](bool value) {
        policy->mutable_allow_redeem_offers()->set_allow_redeem_offers(value);
      });

  EncodeInteger(key::kUptimeLimit, [policy](int value) {
    policy->mutable_uptime_limit()->set_uptime_limit(value);
  });

  EncodeStringList(key::kDeviceStartUpFlags,
                   [policy](const std::vector<std::string>& values) {
                     auto list = policy->mutable_start_up_flags();
                     for (const std::string& value : values)
                       list->add_flags(value);
                   });

  EncodeString(key::kDeviceVariationsRestrictParameter,
               [policy](const std::string& value) {
                 policy->mutable_variations_parameter()->set_parameter(value);
               });

  EncodeBoolean(key::kAttestationEnabledForDevice, [policy](bool value) {
    policy->mutable_attestation_settings()->set_attestation_enabled(value);
  });
  EncodeBoolean(
      key::kAttestationForContentProtectionEnabled, [policy](bool value) {
        policy->mutable_attestation_settings()->set_content_protection_enabled(
            value);
      });

  EncodeString(key::kDeviceLoginScreenPowerManagement,
               [policy](const std::string& value) {
                 policy->mutable_login_screen_power_management()
                     ->set_login_screen_power_management(value);
               });

  EncodeBoolean(key::kDeviceBlockDevmode, [policy](bool value) {
    policy->mutable_system_settings()->set_block_devmode(value);
  });

  EncodeInteger(key::kExtensionCacheSize, [policy](int value) {
    policy->mutable_extension_cache_size()->set_extension_cache_size(value);
  });

  EncodeString(key::kDeviceLoginScreenDomainAutoComplete,
               [policy](const std::string& value) {
                 policy->mutable_login_screen_domain_auto_complete()
                     ->set_login_screen_domain_auto_complete(value);
               });

  EncodeInteger(key::kDisplayRotationDefault, [policy](int value) {
    policy->mutable_display_rotation_default()->set_display_rotation_default(
        static_cast<em::DisplayRotationDefaultProto_Rotation>(value));
  });

  EncodeStringList(
      key::kUsbDetachableWhitelist,
      [policy](const std::vector<std::string>& values) {
        auto list = policy->mutable_usb_detachable_whitelist();
        for (const std::string& value : values) {
          std::string error;
          std::unique_ptr<base::DictionaryValue> dict_value =
              JsonToDictionary(value, &error);
          int vid, pid;
          if (!dict_value || !dict_value->GetInteger("vendor_id", &vid) ||
              !dict_value->GetInteger("product_id", &pid)) {
            LOG(ERROR) << "Invalid JSON string '"
                       << (!error.empty() ? error : value) << "' for policy '"
                       << key::kUsbDetachableWhitelist
                       << "', ignoring. Expected: "
                       << "'{\"vendor_id\"=<vid>, \"product_id\"=<pid>}'.";
            continue;
          }

          em::UsbDeviceIdProto* entry = list->add_id();
          entry->set_vendor_id(vid);
          entry->set_product_id(pid);
        }
      });

  EncodeBoolean(key::kDeviceQuirksDownloadEnabled, [policy](bool value) {
    policy->mutable_quirks_download_enabled()->set_quirks_download_enabled(
        value);
  });

  EncodeString(key::kDeviceWallpaperImage, [policy](const std::string& value) {
    policy->mutable_device_wallpaper_image()->set_device_wallpaper_image(value);
  });
}

void DevicePolicyEncoder::EncodeBoolean(
    const char* policy_name, const BooleanPolicyCallback& set_policy) const {
  // Try to get policy value from dict.
  const base::Value* value = dict_->GetValue(policy_name);
  if (!value)
    return;

  // Get actual value, doing type conversion if necessary.
  bool bool_value;
  if (!helper::GetAsBoolean(value, &bool_value)) {
    helper::PrintConversionError(value, "boolean", policy_name);
    return;
  }

  LOG_IF(INFO, authpolicy::kLogEncoder)
      << " bool " << policy_name << " = " << (bool_value ? "true" : "false");

  // Create proto and set value.
  set_policy(bool_value);
}

void DevicePolicyEncoder::EncodeInteger(
    const char* policy_name, const IntegerPolicyCallback& set_policy) const {
  // Try to get policy value from dict.
  const base::Value* value = dict_->GetValue(policy_name);
  if (!value)
    return;

  // Get actual value, doing type conversion if necessary.
  int int_value;
  if (!helper::GetAsInteger(value, &int_value)) {
    helper::PrintConversionError(value, "integer", policy_name);
    return;
  }

  LOG_IF(INFO, authpolicy::kLogEncoder)
      << " int " << policy_name << " = " << int_value;

  // Create proto and set value.
  set_policy(int_value);
}

void DevicePolicyEncoder::EncodeString(
    const char* policy_name, const StringPolicyCallback& set_policy) const {
  // Try to get policy value from dict.
  const base::Value* value = dict_->GetValue(policy_name);
  if (!value)
    return;

  // Get actual value, doing type conversion if necessary.
  std::string string_value;
  if (!helper::GetAsString(value, &string_value)) {
    helper::PrintConversionError(value, "string", policy_name);
    return;
  }

  LOG_IF(INFO, authpolicy::kLogEncoder)
      << " str " << policy_name << " = " << string_value;

  // Create proto and set value.
  set_policy(string_value);
}

void DevicePolicyEncoder::EncodeStringList(
    const char* policy_name, const StringListPolicyCallback& set_policy) const {
  // Try to get policy key from dict.
  const RegistryDict* key = dict_->GetKey(policy_name);
  if (!key)
    return;

  // Get and check all values. Do this in advance to prevent partial writes.
  std::vector<std::string> string_values;
  for (int index = 0; /* empty */; ++index) {
    std::string indexStr = base::IntToString(index + 1);
    const base::Value* value = key->GetValue(indexStr);
    if (!value)
      break;

    std::string string_value;
    if (!helper::GetAsString(value, &string_value)) {
      helper::PrintConversionError(value, "string", policy_name, &indexStr);
      return;
    }

    string_values.push_back(string_value);
  }

  if (authpolicy::kLogEncoder && LOG_IS_ON(INFO)) {
    LOG(INFO) << " strlist " << policy_name;
    for (const std::string& value : string_values)
      LOG(INFO) << "  " << value;
  }

  // Create proto and set values.
  set_policy(string_values);
}

void DevicePolicyEncoder::HandleUnsupported(const char* policy_name) const {
  // Try to get policy value from dict.
  const base::Value* value = dict_->GetValue(policy_name);
  if (!value)
    return;

  LOG_IF(INFO, authpolicy::kLogEncoder)
      << "Ignoring unsupported policy '" << policy_name << "'.";
}

}  // namespace policy
