// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <chromeos/dbus/service_constants.h>
#include <dbus/dbus.h>
#include <gflags/gflags.h>
#include <glib.h>

#include "update_engine/dbus_constants.h"
#include "update_engine/subprocess.h"
#include "update_engine/utils.h"

extern "C" {
#include "update_engine/update_engine.dbusclient.h"
}

using chromeos_update_engine::kUpdateEngineServiceName;
using chromeos_update_engine::kUpdateEngineServicePath;
using chromeos_update_engine::kUpdateEngineServiceInterface;
using chromeos_update_engine::AttemptUpdateFlags;
using chromeos_update_engine::kAttemptUpdateFlagNonInteractive;
using chromeos_update_engine::utils::GetAndFreeGError;
using std::string;

DEFINE_string(app_version, "", "Force the current app version.");
DEFINE_string(channel, "",
              "Set the target channel. The device will be powerwashed if the "
              "target channel is more stable than the current channel unless "
              "--nopowerwash is specified.");
DEFINE_bool(check_for_update, false, "Initiate check for updates.");
DEFINE_bool(follow, false, "Wait for any update operations to complete."
            "Exit status is 0 if the update succeeded, and 1 otherwise.");
DEFINE_bool(interactive, true, "Mark the update request as interactive.");
DEFINE_string(omaha_url, "", "The URL of the Omaha update server.");
DEFINE_string(p2p_update, "",
              "Enables (\"yes\") or disables (\"no\") the peer-to-peer update "
              "sharing.");
DEFINE_bool(powerwash, true, "When performing rollback or channel change, "
            "do a powerwash or allow it respectively.");
DEFINE_bool(reboot, false, "Initiate a reboot if needed.");
DEFINE_bool(is_reboot_needed, false, "Exit status 0 if reboot is needed, "
            "2 if reboot is not needed or 1 if an error occurred.");
DEFINE_bool(block_until_reboot_is_needed, false, "Blocks until reboot is "
            "needed. Returns non-zero exit status if an error occurred.");
DEFINE_bool(reset_status, false, "Sets the status in update_engine to idle.");
DEFINE_bool(rollback, false, "Perform a rollback to the previous partition.");
DEFINE_bool(can_rollback, false, "Shows whether rollback partition "
            "is available.");
DEFINE_bool(show_channel, false, "Show the current and target channels.");
DEFINE_bool(show_p2p_update, false,
            "Show the current setting for peer-to-peer update sharing.");
DEFINE_bool(show_update_over_cellular, false,
            "Show the current setting for updates over cellular networks.");
DEFINE_bool(status, false, "Print the status to stdout.");
DEFINE_bool(update, false, "Forces an update and waits for it to complete. "
            "Implies --follow.");
DEFINE_string(update_over_cellular, "",
              "Enables (\"yes\") or disables (\"no\") the updates over "
              "cellular networks.");
DEFINE_bool(watch_for_updates, false,
            "Listen for status updates and print them to the screen.");
DEFINE_bool(prev_version, false,
            "Show the previous OS version used before the update reboot.");
DEFINE_bool(show_kernels, false, "Show the list of kernel patritions and "
            "whether each of them is bootable or not");

namespace {

bool GetProxy(DBusGProxy** out_proxy) {
  DBusGConnection* bus;
  DBusGProxy* proxy = NULL;
  GError* error = NULL;
  const int kTries = 4;
  const int kRetrySeconds = 10;

  bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
  if (bus == NULL) {
    LOG(ERROR) << "Failed to get bus: " << GetAndFreeGError(&error);
    exit(1);
  }
  for (int i = 0; !proxy && i < kTries; ++i) {
    if (i > 0) {
      LOG(INFO) << "Retrying to get dbus proxy. Try "
                << (i + 1) << "/" << kTries;
      g_usleep(kRetrySeconds * G_USEC_PER_SEC);
    }
    proxy = dbus_g_proxy_new_for_name_owner(bus,
                                            kUpdateEngineServiceName,
                                            kUpdateEngineServicePath,
                                            kUpdateEngineServiceInterface,
                                            &error);
    LOG_IF(WARNING, !proxy) << "Error getting dbus proxy for "
                            << kUpdateEngineServiceName << ": "
                            << GetAndFreeGError(&error);
  }
  if (proxy == NULL) {
    LOG(ERROR) << "Giving up -- unable to get dbus proxy for "
               << kUpdateEngineServiceName;
    exit(1);
  }
  *out_proxy = proxy;
  return true;
}

static void StatusUpdateSignalHandler(DBusGProxy* proxy,
                                      int64_t last_checked_time,
                                      double progress,
                                      gchar* current_operation,
                                      gchar* new_version,
                                      int64_t new_size,
                                      void* user_data) {
  LOG(INFO) << "Got status update:";
  LOG(INFO) << "  last_checked_time: " << last_checked_time;
  LOG(INFO) << "  progress: " << progress;
  LOG(INFO) << "  current_operation: " << current_operation;
  LOG(INFO) << "  new_version: " << new_version;
  LOG(INFO) << "  new_size: " << new_size;
}

bool ResetStatus() {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean rc = update_engine_client_reset_status(proxy, &error);
  return rc;
}


// If |op| is non-NULL, sets it to the current operation string or an
// empty string if unable to obtain the current status.
bool GetStatus(string* op) {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gint64 last_checked_time = 0;
  gdouble progress = 0.0;
  char* current_op = NULL;
  char* new_version = NULL;
  gint64 new_size = 0;

  gboolean rc = update_engine_client_get_status(proxy,
                                                &last_checked_time,
                                                &progress,
                                                &current_op,
                                                &new_version,
                                                &new_size,
                                                &error);
  if (rc == FALSE) {
    LOG(INFO) << "Error getting status: " << GetAndFreeGError(&error);
  }
  printf("LAST_CHECKED_TIME=%" PRIi64 "\nPROGRESS=%f\nCURRENT_OP=%s\n"
         "NEW_VERSION=%s\nNEW_SIZE=%" PRIi64 "\n",
         last_checked_time,
         progress,
         current_op,
         new_version,
         new_size);
  if (op) {
    *op = current_op ? current_op : "";
  }
  return true;
}

// Should never return.
void WatchForUpdates() {
  DBusGProxy* proxy;

  CHECK(GetProxy(&proxy));

  // Register marshaller
  dbus_g_object_register_marshaller(
      g_cclosure_marshal_generic,
      G_TYPE_NONE,
      G_TYPE_INT64,
      G_TYPE_DOUBLE,
      G_TYPE_STRING,
      G_TYPE_STRING,
      G_TYPE_INT64,
      G_TYPE_INVALID);

  static const char kStatusUpdate[] = "StatusUpdate";
  dbus_g_proxy_add_signal(proxy,
                          kStatusUpdate,
                          G_TYPE_INT64,
                          G_TYPE_DOUBLE,
                          G_TYPE_STRING,
                          G_TYPE_STRING,
                          G_TYPE_INT64,
                          G_TYPE_INVALID);
  GMainLoop* loop = g_main_loop_new(NULL, TRUE);
  dbus_g_proxy_connect_signal(proxy,
                              kStatusUpdate,
                              G_CALLBACK(StatusUpdateSignalHandler),
                              NULL,
                              NULL);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

bool Rollback(bool rollback) {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean rc = update_engine_client_attempt_rollback(proxy,
                                                      rollback,
                                                      &error);
  CHECK_EQ(rc, TRUE) << "Error with rollback request: "
                     << GetAndFreeGError(&error);
  return true;
}

std::string GetRollbackPartition() {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  char* rollback_partition = nullptr;
  gboolean rc = update_engine_client_get_rollback_partition(proxy,
                                                            &rollback_partition,
                                                            &error);
  CHECK_EQ(rc, TRUE) << "Error while querying rollback partition availabilty: "
                     << GetAndFreeGError(&error);
  std::string partition = rollback_partition;
  g_free(rollback_partition);
  return partition;
}

std::string GetKernelDevices() {
  DBusGProxy* proxy;
  GError* error = nullptr;

  CHECK(GetProxy(&proxy));

  char* kernel_devices = nullptr;
  gboolean rc = update_engine_client_get_kernel_devices(proxy,
                                                        &kernel_devices,
                                                        &error);
  CHECK_EQ(rc, TRUE) << "Error while getting a list of kernel devices: "
    << GetAndFreeGError(&error);
  std::string devices = kernel_devices;
  g_free(kernel_devices);
  return devices;
}

bool CheckForUpdates(const string& app_version, const string& omaha_url) {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  AttemptUpdateFlags flags = static_cast<AttemptUpdateFlags>(
      FLAGS_interactive ? 0 : kAttemptUpdateFlagNonInteractive);
  gboolean rc =
      update_engine_client_attempt_update_with_flags(proxy,
                                                     app_version.c_str(),
                                                     omaha_url.c_str(),
                                                     static_cast<gint>(flags),
                                                     &error);
  CHECK_EQ(rc, TRUE) << "Error checking for update: "
                     << GetAndFreeGError(&error);
  return true;
}

bool RebootIfNeeded() {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean rc =
      update_engine_client_reboot_if_needed(proxy, &error);
  // Reboot error code doesn't necessarily mean that a reboot
  // failed. For example, D-Bus may be shutdown before we receive the
  // result.
  LOG_IF(INFO, !rc) << "Reboot error message: " << GetAndFreeGError(&error);
  return true;
}

void SetTargetChannel(const string& target_channel, bool allow_powerwash) {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean rc = update_engine_client_set_channel(proxy,
                                                 target_channel.c_str(),
                                                 allow_powerwash,
                                                 &error);
  CHECK_EQ(rc, true) << "Error setting the channel: "
                     << GetAndFreeGError(&error);
  LOG(INFO) << "Channel permanently set to: " << target_channel;
}

string GetChannel(bool get_current_channel) {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  char* channel = NULL;
  gboolean rc = update_engine_client_get_channel(proxy,
                                                 get_current_channel,
                                                 &channel,
                                                 &error);
  CHECK_EQ(rc, true) << "Error getting the channel: "
                     << GetAndFreeGError(&error);
  string output = channel;
  g_free(channel);
  return output;
}

void SetUpdateOverCellularPermission(gboolean allowed) {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean rc = update_engine_client_set_update_over_cellular_permission(
      proxy,
      allowed,
      &error);
  CHECK_EQ(rc, true) << "Error setting the update over cellular setting: "
                     << GetAndFreeGError(&error);
}

bool GetUpdateOverCellularPermission() {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean allowed;
  gboolean rc = update_engine_client_get_update_over_cellular_permission(
      proxy,
      &allowed,
      &error);
  CHECK_EQ(rc, true) << "Error getting the update over cellular setting: "
                     << GetAndFreeGError(&error);
  return allowed;
}

void SetP2PUpdatePermission(gboolean enabled) {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean rc = update_engine_client_set_p2p_update_permission(
      proxy,
      enabled,
      &error);
  CHECK_EQ(rc, true) << "Error setting the peer-to-peer update setting: "
                     << GetAndFreeGError(&error);
}

bool GetP2PUpdatePermission() {
  DBusGProxy* proxy;
  GError* error = NULL;

  CHECK(GetProxy(&proxy));

  gboolean enabled;
  gboolean rc = update_engine_client_get_p2p_update_permission(
      proxy,
      &enabled,
      &error);
  CHECK_EQ(rc, true) << "Error getting the peer-to-peer update setting: "
                     << GetAndFreeGError(&error);
  return enabled;
}

static gboolean CompleteUpdateSource(gpointer data) {
  string current_op;
  if (!GetStatus(&current_op) || current_op == "UPDATE_STATUS_IDLE") {
    LOG(ERROR) << "Update failed.";
    exit(1);
  }
  if (current_op == "UPDATE_STATUS_UPDATED_NEED_REBOOT") {
    LOG(INFO) << "Update succeeded -- reboot needed.";
    exit(0);
  }
  return TRUE;
}

// This is similar to watching for updates but rather than registering
// a signal watch, actively poll the daemon just in case it stops
// sending notifications.
void CompleteUpdate() {
  GMainLoop* loop = g_main_loop_new(NULL, TRUE);
  g_timeout_add_seconds(5, CompleteUpdateSource, NULL);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

void ShowPrevVersion() {
  DBusGProxy* proxy;
  GError* error = nullptr;

  CHECK(GetProxy(&proxy));

  char* prev_version = nullptr;

  gboolean rc = update_engine_client_get_prev_version(proxy,
                                                      &prev_version,
                                                      &error);
  if (!rc) {
    LOG(ERROR) << "Error getting previous version: "
               << GetAndFreeGError(&error);
  } else {
    LOG(INFO) << "Previous version = " << prev_version;
    g_free(prev_version);
  }
}

bool CheckIfRebootIsNeeded(DBusGProxy *proxy, bool *out_reboot_needed) {
  gint64 last_checked_time = 0;
  gdouble progress = 0.0;
  char* current_op = nullptr;
  char* new_version = nullptr;
  gint64 new_size = 0;
  GError* error = nullptr;

  if (!update_engine_client_get_status(proxy,
                                       &last_checked_time,
                                       &progress,
                                       &current_op,
                                       &new_version,
                                       &new_size,
                                       &error)) {
    LOG(INFO) << "Error getting status: " << GetAndFreeGError(&error);
    return false;
  }
  *out_reboot_needed =
      (g_strcmp0(current_op,
                 update_engine::kUpdateStatusUpdatedNeedReboot) == 0);
  g_free(current_op);
  g_free(new_version);
  return true;
}

// Determines if reboot is needed. The result is returned in
// |out_reboot_needed|. Returns true if the check succeeded, false
// otherwise.
bool IsRebootNeeded(bool *out_reboot_needed) {
  DBusGProxy* proxy = nullptr;
  CHECK(GetProxy(&proxy));
  bool ret = CheckIfRebootIsNeeded(proxy, out_reboot_needed);
  g_object_unref(proxy);
  return ret;
}

static void OnBlockUntilRebootStatusCallback(
    DBusGProxy* proxy,
    int64_t last_checked_time,
    double progress,
    const gchar* current_operation,
    const gchar* new_version,
    int64_t new_size,
    void* user_data) {
  GMainLoop *loop = reinterpret_cast<GMainLoop*>(user_data);
  if (g_strcmp0(current_operation,
                update_engine::kUpdateStatusUpdatedNeedReboot) == 0) {
    g_main_loop_quit(loop);
  }
}

bool CheckRebootNeeded(DBusGProxy *proxy, GMainLoop *loop) {
  bool reboot_needed;
  if (!CheckIfRebootIsNeeded(proxy, &reboot_needed))
    return false;
  if (reboot_needed)
    return true;
  // This will block until OnBlockUntilRebootStatusCallback() calls
  // g_main_loop_quit().
  g_main_loop_run(loop);
  return true;
}

// Blocks until a reboot is needed. Returns true if waiting succeeded,
// false if an error occurred.
bool BlockUntilRebootIsNeeded() {
  // The basic idea is to get a proxy, listen to signals and only then
  // check the status. If no reboot is needed, just sit and wait for
  // the StatusUpdate signal to convey that a reboot is pending.
  DBusGProxy* proxy = nullptr;
  CHECK(GetProxy(&proxy));
  dbus_g_object_register_marshaller(
      g_cclosure_marshal_generic,
      G_TYPE_NONE,
      G_TYPE_INT64,
      G_TYPE_DOUBLE,
      G_TYPE_STRING,
      G_TYPE_STRING,
      G_TYPE_INT64,
      G_TYPE_INVALID);
  dbus_g_proxy_add_signal(proxy,
                          update_engine::kStatusUpdate,  // Signal name.
                          G_TYPE_INT64,
                          G_TYPE_DOUBLE,
                          G_TYPE_STRING,
                          G_TYPE_STRING,
                          G_TYPE_INT64,
                          G_TYPE_INVALID);
  GMainLoop* loop = g_main_loop_new(nullptr, TRUE);
  dbus_g_proxy_connect_signal(proxy,
                              update_engine::kStatusUpdate,
                              G_CALLBACK(OnBlockUntilRebootStatusCallback),
                              loop,
                              nullptr);  // free_data_func.

  bool ret = CheckRebootNeeded(proxy, loop);

  dbus_g_proxy_disconnect_signal(proxy,
                                 update_engine::kStatusUpdate,
                                 G_CALLBACK(OnBlockUntilRebootStatusCallback),
                                 loop);
  g_main_loop_unref(loop);
  g_object_unref(proxy);
  return ret;
}

}  // namespace

int main(int argc, char** argv) {
  // Boilerplate init commands.
  g_type_init();
  dbus_threads_init_default();
  chromeos_update_engine::Subprocess::Init();
  google::ParseCommandLineFlags(&argc, &argv, true);

  // Update the status if requested.
  if (FLAGS_reset_status) {
    LOG(INFO) << "Setting Update Engine status to idle ...";
    if (!ResetStatus()) {
      LOG(ERROR) << "ResetStatus failed.";
      return 1;
    }
    LOG(INFO) << "ResetStatus succeeded; to undo partition table changes run:\n"
                 "(D=$(rootdev -d) P=$(rootdev -s); cgpt p -i$(($(echo ${P#$D} "
                 "| sed 's/^[^0-9]*//')-1)) $D;)";
  }

  // Changes the current update over cellular network setting.
  if (!FLAGS_update_over_cellular.empty()) {
    gboolean allowed = FLAGS_update_over_cellular == "yes";
    if (!allowed && FLAGS_update_over_cellular != "no") {
      LOG(ERROR) << "Unknown option: \"" << FLAGS_update_over_cellular
                 << "\". Please specify \"yes\" or \"no\".";
    } else {
      SetUpdateOverCellularPermission(allowed);
    }
  }

  // Show the current update over cellular network setting.
  if (FLAGS_show_update_over_cellular) {
    bool allowed = GetUpdateOverCellularPermission();
    LOG(INFO) << "Current update over cellular network setting: "
              << (allowed ? "ENABLED" : "DISABLED");
  }

  if (!FLAGS_powerwash && !FLAGS_rollback && FLAGS_channel.empty()) {
    LOG(ERROR) << "powerwash flag only makes sense rollback or channel change";
    return 1;
  }

  // Change the P2P enabled setting.
  if (!FLAGS_p2p_update.empty()) {
    gboolean enabled = FLAGS_p2p_update == "yes";
    if (!enabled && FLAGS_p2p_update != "no") {
      LOG(ERROR) << "Unknown option: \"" << FLAGS_p2p_update
                 << "\". Please specify \"yes\" or \"no\".";
    } else {
      SetP2PUpdatePermission(enabled);
    }
  }

  // Show the rollback availability.
  if (FLAGS_can_rollback) {
    std::string rollback_partition = GetRollbackPartition();
    bool can_rollback = true;
    if (rollback_partition.empty()) {
      rollback_partition = "UNAVAILABLE";
      can_rollback = false;
    } else {
      rollback_partition = "AVAILABLE: " + rollback_partition;
    }

    LOG(INFO) << "Rollback partition: " << rollback_partition;
    if (!can_rollback) {
      return 1;
    }
  }

  // Show the current P2P enabled setting.
  if (FLAGS_show_p2p_update) {
    bool enabled = GetP2PUpdatePermission();
    LOG(INFO) << "Current update using P2P setting: "
              << (enabled ? "ENABLED" : "DISABLED");
  }

  // First, update the target channel if requested.
  if (!FLAGS_channel.empty())
    SetTargetChannel(FLAGS_channel, FLAGS_powerwash);

  // Show the current and target channels if requested.
  if (FLAGS_show_channel) {
    string current_channel = GetChannel(true);
    LOG(INFO) << "Current Channel: " << current_channel;

    string target_channel = GetChannel(false);
    if (!target_channel.empty())
      LOG(INFO) << "Target Channel (pending update): " << target_channel;
  }

  bool do_update_request = FLAGS_check_for_update | FLAGS_update |
      !FLAGS_app_version.empty() | !FLAGS_omaha_url.empty();
  if (FLAGS_update)
    FLAGS_follow = true;

  if (do_update_request && FLAGS_rollback) {
    LOG(ERROR) << "Incompatible flags specified with rollback."
               << "Rollback should not include update-related flags.";
    return 1;
  }

  if (FLAGS_rollback) {
    LOG(INFO) << "Requesting rollback.";
    CHECK(Rollback(FLAGS_powerwash)) << "Request for rollback failed.";
  }

  // Initiate an update check, if necessary.
  if (do_update_request) {
    LOG_IF(WARNING, FLAGS_reboot) << "-reboot flag ignored.";
    string app_version = FLAGS_app_version;
    if (FLAGS_update && app_version.empty()) {
      app_version = "ForcedUpdate";
      LOG(INFO) << "Forcing an update by setting app_version to ForcedUpdate.";
    }
    LOG(INFO) << "Initiating update check and install.";
    CHECK(CheckForUpdates(app_version, FLAGS_omaha_url))
        << "Update check/initiate update failed.";
  }

  // These final options are all mutually exclusive with one another.
  if (FLAGS_follow + FLAGS_watch_for_updates + FLAGS_reboot +
      FLAGS_status + FLAGS_is_reboot_needed +
      FLAGS_block_until_reboot_is_needed > 1) {
    LOG(ERROR) << "Multiple exclusive options selected. "
               << "Select only one of --follow, --watch_for_updates, --reboot, "
               << "--is_reboot_needed, --block_until_reboot_is_needed, "
               << "or --status.";
    return 1;
  }

  if (FLAGS_status) {
    LOG(INFO) << "Querying Update Engine status...";
    if (!GetStatus(NULL)) {
      LOG(ERROR) << "GetStatus failed.";
      return 1;
    }
    return 0;
  }

  if (FLAGS_follow) {
    LOG(INFO) << "Waiting for update to complete.";
    CompleteUpdate();  // Should never return.
    return 1;
  }

  if (FLAGS_watch_for_updates) {
    LOG(INFO) << "Watching for status updates.";
    WatchForUpdates();  // Should never return.
    return 1;
  }

  if (FLAGS_reboot) {
    LOG(INFO) << "Requesting a reboot...";
    CHECK(RebootIfNeeded());
    return 0;
  }

  if (FLAGS_prev_version) {
    ShowPrevVersion();
  }

  if (FLAGS_show_kernels) {
    LOG(INFO) << "Kernel partitions:\n"
              << GetKernelDevices();
  }

  if (FLAGS_is_reboot_needed) {
    bool reboot_needed = false;
    if (!IsRebootNeeded(&reboot_needed))
      return 1;
    else if (!reboot_needed)
      return 2;
  }

  if (FLAGS_block_until_reboot_is_needed) {
    if (!BlockUntilRebootIsNeeded())
      return 1;
  }

  LOG(INFO) << "Done.";
  return 0;
}
