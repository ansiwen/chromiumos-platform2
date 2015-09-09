// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_SERVER_TPM_INITIALIZER_IMPL_H_
#define TPM_MANAGER_SERVER_TPM_INITIALIZER_IMPL_H_

#include <string>

#include <base/macros.h>
#include <trousers/tss.h>
#include <trousers/trousers.h>  // NOLINT(build/include_alpha)

#include "tpm_manager/server/openssl_crypto_util.h"
#include "tpm_manager/server/tpm_connection.h"
#include "tpm_manager/server/tpm_initializer.h"

namespace tpm_manager {

class LocalDataStore;
class TpmStatus;

// This class initializes a Tpm1.2 chip by taking ownership. Example use of
// this class is:
// LocalDataStore data_store;
// TpmStatusImpl status;
// TpmInitializerImpl initializer(&data_store, &status);
// initializer.InitializeTpm();
// If the tpm is unowned, InitializeTpm injects a random owner password,
// initializes and unrestricts the SRK, and persists the owner password to disk
// until all the owner dependencies are satisfied.
class TpmInitializerImpl : public TpmInitializer {
 public:
  // Does not take ownership of |local_data_store| or |tpm_status|.
  TpmInitializerImpl(LocalDataStore* local_data_store,
                     TpmStatus* tpm_status);
  ~TpmInitializerImpl() override = default;

  // TpmInitializer methods.
  bool InitializeTpm() override;

 private:
  // This method checks if an EndorsementKey exists on the Tpm and creates it
  // if not. Returns true on success, else false. |tpm_handle| is a handle to
  // the Tpm with the owner_password injected.
  bool InitializeEndorsementKey(TSS_HTPM tpm_handle);

  // This method takes ownership of the Tpm with the default TSS password.
  // Returns true on success, else false. |tpm_handle| is a handle to the Tpm
  // with the owner_password injected.
  bool TakeOwnership(TSS_HTPM tpm_handle);

  // This method initializes the SRK if it does not exist, zero's the SRK
  // password and unrestricts its usage. Returns true on success, else false.
  // |tpm_handle| is a handle to the Tpm with the owner_password injected.
  bool InitializeSrk(TSS_HTPM tpm_handle);

  // This method changes the Tpm owner password from the default TSS password
  // to the password provided in the |owner_password| argument.
  // Returns true on success, else false. |tpm_handle| is a handle to the Tpm
  // with the old owner_password injected.
  bool ChangeOwnerPassword(TSS_HTPM tpm_handle,
                           const std::string& owner_password);

  // This method return true iff the provided |owner_password| is the current
  // owner password in the Tpm. This method can also return false if there was
  // an error communicating with the Tpm.
  bool TestTpmAuth(const std::string& owner_password);

  OpensslCryptoUtil openssl_util_;
  TpmConnection tpm_connection_;
  LocalDataStore* local_data_store_;
  TpmStatus* tpm_status_;

  DISALLOW_COPY_AND_ASSIGN(TpmInitializerImpl);
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_SERVER_TPM_INITIALIZER_IMPL_H_