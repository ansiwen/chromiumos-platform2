// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_AUTHPOLICY_H_
#define AUTHPOLICY_AUTHPOLICY_H_

#include <memory>
#include <string>

#include <brillo/dbus/async_event_sequencer.h>
#include <dbus/object_proxy.h>

#include "authpolicy/org.chromium.AuthPolicy.h"
#include "authpolicy/samba_interface.h"

using brillo::dbus_utils::AsyncEventSequencer;

namespace enterprise_management {
class CloudPolicySettings;
class ChromeDeviceSettingsProto;
class PolicyData;
}

namespace authpolicy {

class AuthPolicy : public org::chromium::AuthPolicyAdaptor,
                   public org::chromium::AuthPolicyInterface {
 public:
  using PolicyResponseCallback =
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<int32_t>>;

  explicit AuthPolicy(
      brillo::dbus_utils::ExportedObjectManager* object_manager);
  ~AuthPolicy() override = default;

  // Register the D-Bus object and interfaces.
  void RegisterAsync(
      const AsyncEventSequencer::CompletionAction& completion_callback);

  // org::chromium::AuthPolicyInterface: (see org.chromium.AuthPolicy.xml)
  bool AuthenticateUser(brillo::ErrorPtr* error,
                        const std::string& in_user_principal_name,
                        const dbus::FileDescriptor& in_password_fd,
                        int32_t* out_error_code,
                        std::string* out_account_id) override;
  bool JoinADDomain(brillo::ErrorPtr* error, const std::string& in_machine_name,
                    const std::string& in_user_principal_name,
                    const dbus::FileDescriptor& in_password_fd,
                    int32_t* out_error_code) override;
  void RefreshUserPolicy(PolicyResponseCallback callback,
                         const std::string& in_account_id) override;
  void RefreshDevicePolicy(PolicyResponseCallback callback) override;

 private:
  // Sends the user policy blob to SessionManager.
  void StoreUserPolicy(const std::string& account_id,
                       const enterprise_management::CloudPolicySettings& policy,
                       PolicyResponseCallback callback);

  // Sends the device policy blob to SessionManager.
  void StoreDevicePolicy(
      const enterprise_management::ChromeDeviceSettingsProto& policy,
      PolicyResponseCallback callback);

  // Sends policy to SessionManager. Assumes |policy_data| contains user policy
  // if account_id is not nullptr, otherwise assumes it's device policy.
  void StorePolicy(const enterprise_management::PolicyData& policy_data,
                   const std::string* account_id,
                   PolicyResponseCallback callback);

  // Response callback from SessionManager, logs the result and calls callback.
  void OnPolicyStored(const char* method, PolicyResponseCallback callback,
                      dbus::Response* response);

  SambaInterface samba_;

  brillo::dbus_utils::DBusObject dbus_object_;
  dbus::ObjectProxy* session_manager_proxy_;
  base::WeakPtrFactory<AuthPolicy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthPolicy);
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_AUTHPOLICY_H_