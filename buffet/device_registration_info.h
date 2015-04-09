// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_DEVICE_REGISTRATION_INFO_H_
#define BUFFET_DEVICE_REGISTRATION_INFO_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/time/time.h>
#include <base/timer/timer.h>
#include <chromeos/data_encoding.h>
#include <chromeos/errors/error.h>
#include <chromeos/http/http_transport.h>

#include "buffet/buffet_config.h"
#include "buffet/registration_status.h"
#include "buffet/storage_interface.h"
#include "buffet/xmpp/xmpp_client.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace chromeos {
class KeyValueStore;
}  // namespace chromeos

namespace buffet {

class CommandManager;
class StateManager;

extern const char kErrorDomainOAuth2[];
extern const char kErrorDomainGCD[];
extern const char kErrorDomainGCDServer[];

// The DeviceRegistrationInfo class represents device registration information.
class DeviceRegistrationInfo : public base::MessageLoopForIO::Watcher {
 public:
  // This is a helper class for unit testing.
  class TestHelper;

  DeviceRegistrationInfo(
      const std::shared_ptr<CommandManager>& command_manager,
      const std::shared_ptr<StateManager>& state_manager,
      std::unique_ptr<const BuffetConfig> config,
      const std::shared_ptr<chromeos::http::Transport>& transport,
      const std::shared_ptr<StorageInterface>& state_store,
      bool xmpp_enabled,
      const base::Closure& on_status_changed);

  ~DeviceRegistrationInfo() override;

  void OnFileCanReadWithoutBlocking(int fd) override;

  void OnFileCanWriteWithoutBlocking(int fd) override {
    LOG(FATAL) << "No write watcher is configured";
  }

  // Returns our current best known registration status.
  RegistrationStatus GetRegistrationStatus() const {
    return registration_status_;
  }

  // Returns the authorization HTTP header that can be used to talk
  // to GCD server for authenticated device communication.
  // Make sure ValidateAndRefreshAccessToken() is called before this call.
  std::pair<std::string, std::string> GetAuthorizationHeader() const;

  // Returns the GCD service request URL. If |subpath| is specified, it is
  // appended to the base URL which is normally
  //    https://www.googleapis.com/clouddevices/v1/".
  // If |params| are specified, each key-value pair is formatted using
  // chromeos::data_encoding::WebParamsEncode() and appended to URL as a query
  // string.
  // So, calling:
  //    GetServiceURL("ticket", {{"key","apiKey"}})
  // will return something like:
  //    https://www.googleapis.com/clouddevices/v1/ticket?key=apiKey
  std::string GetServiceURL(
      const std::string& subpath = {},
      const chromeos::data_encoding::WebParamList& params = {}) const;

  // Returns a service URL to access the registered device on GCD server.
  // The base URL used to construct the full URL looks like this:
  //    https://www.googleapis.com/clouddevices/v1/devices/<device_id>/
  std::string GetDeviceURL(
    const std::string& subpath = {},
    const chromeos::data_encoding::WebParamList& params = {}) const;

  // Similar to GetServiceURL, GetOAuthURL() returns a URL of OAuth 2.0 server.
  // The base URL used is https://accounts.google.com/o/oauth2/.
  std::string GetOAuthURL(
    const std::string& subpath = {},
    const chromeos::data_encoding::WebParamList& params = {}) const;

  // Returns the registered device ID (GUID) or empty string.
  const std::string& GetDeviceId() const;

  // Loads the device registration information from cache.
  bool Load();

  // Checks for the valid device registration as well as refreshes
  // the device access token, if available.
  bool CheckRegistration(chromeos::ErrorPtr* error);

  // Gets the full device description JSON object, or nullptr if
  // the device is not registered or communication failure.
  std::unique_ptr<base::DictionaryValue> GetDeviceInfo(
      chromeos::ErrorPtr* error);

  // Registers the device.
  //
  // |params| are a list of key-value pairs of device information,
  // such as client_id, client_secret, and so on. If a particular key-value pair
  // is omitted, a default value is used when possible.
  // Returns a device ID on success.
  // The values are all strings for now.
  std::string RegisterDevice(
    const std::map<std::string, std::string>& params,
    chromeos::ErrorPtr* error);

  // Updates a command.
  // TODO(antonm): Should solve the issues with async vs. sync.
  // TODO(antonm): Consider moving some other class.
  void UpdateCommand(const std::string& command_id,
                     const base::DictionaryValue& command_patch);

 private:
  // Cause DeviceRegistrationInfo to attempt to StartDevice on its own later.
  void ScheduleStartDevice(const base::TimeDelta& later);

  // Starts device execution.
  // Device will do required start up chores and then start to listen
  // to new commands.
  // TODO(antonm): Consider moving into some other class.
  void StartDevice(chromeos::ErrorPtr* error,
                   const base::TimeDelta& retry_delay);

  // Saves the device registration to cache.
  bool Save() const;

  // Checks whether we have credentials generated during registration.
  bool HaveRegistrationCredentials(chromeos::ErrorPtr* error);

  // If we currently have an access token and it doesn't like like it
  // has expired yet, returns true immediately. Otherwise calls
  // RefreshAccessToken().
  bool MaybeRefreshAccessToken(chromeos::ErrorPtr* error);

  // Forcibly refreshes the access token.
  bool RefreshAccessToken(chromeos::ErrorPtr* error);

  // Parse the OAuth response, and sets registration status to
  // kInvalidCredentials if our registration is no longer valid.
  std::unique_ptr<base::DictionaryValue> ParseOAuthResponse(
      chromeos::http::Response* response, chromeos::ErrorPtr* error);

  // This attempts to open the XMPP channel. The XMPP channel needs to be
  // restarted anytime the access_token is refreshed.
  void StartXmpp();

  using CloudRequestCallback =
      base::Callback<void(const base::DictionaryValue&)>;
  using CloudRequestErrorCallback =
      base::Callback<void(const chromeos::Error* error)>;

  // Do a HTTPS request to cloud services.
  // Handles many cases like reauthorization, 5xx HTTP response codes
  // and device removal.  It is a recommended way to do cloud API
  // requests.
  // TODO(antonm): Consider moving into some other class.
  void DoCloudRequest(
      const std::string& method,
      const std::string& url,
      const base::DictionaryValue* body,
      const CloudRequestCallback& success_callback,
      const CloudRequestErrorCallback& error_callback);

  void UpdateDeviceResource(const base::Closure& on_success,
                            const CloudRequestErrorCallback& on_failure);

  void FetchCommands(
      const base::Callback<void(const base::ListValue&)>& on_success,
      const CloudRequestErrorCallback& on_failure);

  void AbortLimboCommands(const base::Closure& callback,
                          const base::ListValue& commands);

  void PeriodicallyPollCommands();

  void PublishCommands(const base::ListValue& commands);

  void PublishStateUpdates();

  // Builds Cloud API devices collection REST resouce which matches
  // current state of the device including command definitions
  // for all supported commands and current device state.
  std::unique_ptr<base::DictionaryValue> BuildDeviceResource(
      chromeos::ErrorPtr* error);

  void SetRegistrationStatus(RegistrationStatus new_status);
  void SetDeviceId(const std::string& device_id);

  // Data that is cached here, persisted in the state store.
  std::string refresh_token_;
  std::string device_id_;
  std::string device_robot_account_;

  // These fields are user settable and stored in the state store.
  std::string name_;
  std::string description_;
  std::string location_;

  // Transient data
  std::string access_token_;
  base::Time access_token_expiration_;

  // HTTP transport used for communications.
  std::shared_ptr<chromeos::http::Transport> transport_;
  // Serialization interface to save and load device registration info.
  std::shared_ptr<StorageInterface> storage_;
  // Global command manager.
  std::shared_ptr<CommandManager> command_manager_;
  // Device state manager.
  std::shared_ptr<StateManager> state_manager_;

  std::unique_ptr<const BuffetConfig> config_;

  const bool xmpp_enabled_;
  std::unique_ptr<XmppClient> xmpp_client_;
  base::MessageLoopForIO::FileDescriptorWatcher fd_watcher_;

  // Tracks our current registration status.
  RegistrationStatus registration_status_{RegistrationStatus::kUnconfigured};
  base::Closure on_status_changed_;

  base::RepeatingTimer<DeviceRegistrationInfo> command_poll_timer_;
  base::RepeatingTimer<DeviceRegistrationInfo> state_push_timer_;

  friend class TestHelper;
  base::WeakPtrFactory<DeviceRegistrationInfo> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DeviceRegistrationInfo);
};

}  // namespace buffet

#endif  // BUFFET_DEVICE_REGISTRATION_INFO_H_
