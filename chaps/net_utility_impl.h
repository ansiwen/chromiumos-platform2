// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_NET_UTILITY_IMPL_H_
#define CHAPS_NET_UTILITY_IMPL_H_

#include "net_utility.h"

#include <string>
#include <boost/optional.hpp>
#include <cpprest/http_client.h>
#include <base/macros.h>

namespace chaps {

class ObjectPool;
class ChapsFactory;

class NetUtilityImpl : public NetUtility {
 public:
  explicit NetUtilityImpl(std::shared_ptr<ObjectPool> token_object_pool,
                          std::shared_ptr<ChapsFactory> factory);
  virtual ~NetUtilityImpl();
  virtual bool Init();
  virtual bool LoadKeys(const std::string& key_id);
  virtual boost::optional<std::string> Decrypt(const std::string& key_id,
                                               const std::string& input);
  // virtual boost::optional<std::string> Sign(const std::string& key_id,
  //                                           const std::string& input);

 private:
  bool is_initialized_;
  boost::optional<web::http::client::http_client> client_;
  std::shared_ptr<ObjectPool> token_object_pool_;
  std::shared_ptr<ChapsFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(NetUtilityImpl);
};

}  // namespace chaps

#endif  // CHAPS_TPM_UTILITY_NETHSM_H_
