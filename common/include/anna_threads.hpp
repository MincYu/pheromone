//  Copyright 2019 U.C. Berkeley RISE Lab
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#ifndef INCLUDE_ANNA_THREADS_HPP_
#define INCLUDE_ANNA_THREADS_HPP_

#include "types.hpp"

// The port on which clients send key address requests to routing nodes.
const unsigned kKeyAddressPort = 6450;

// The port on which clients receive responses from the KVS.
const unsigned kUserResponsePort = 6800;

// The port on which clients receive responses from the routing tier.
const unsigned kUserKeyAddressPort = 6850;

// The port on which cache nodes listen for updates from the KVS.
const unsigned kCacheUpdatePort = 7150;

const string kBindBase = "tcp://*:";

class CacheThread {
  Address ip_;
  Address ip_base_;
  unsigned tid_;

 public:
  CacheThread(Address ip, unsigned tid) :
      ip_(ip),
      ip_base_("tcp://" + ip_ + ":"),
      tid_(tid) {}

  Address ip() const { return ip_; }

  unsigned tid() const { return tid_; }

  Address cache_get_bind_address() const { return "ipc:///requests/get"; }

  Address cache_get_connect_address() const { return "ipc:///requests/get"; }

  Address cache_put_bind_address() const { return "ipc:///requests/put"; }

  Address cache_put_connect_address() const { return "ipc:///requests/put"; }

  Address cache_update_bind_address() const {
    return kBindBase + std::to_string(tid_ + kCacheUpdatePort);
  }

  Address cache_update_connect_address() const {
    return ip_base_ + std::to_string(tid_ + kCacheUpdatePort);
  }
};

class UserRoutingThread {
  Address ip_;
  Address ip_base_;
  unsigned tid_;

 public:
  UserRoutingThread() {}

  UserRoutingThread(Address ip, unsigned tid) :
      ip_(ip),
      tid_(tid),
      ip_base_("tcp://" + ip_ + ":") {}

  Address ip() const { return ip_; }

  unsigned tid() const { return tid_; }

  Address key_address_connect_address() const {
    return ip_base_ + std::to_string(tid_ + kKeyAddressPort);
  }

  Address key_address_bind_address() const {
    return kBindBase + std::to_string(tid_ + kKeyAddressPort);
  }
};

class UserThread {
  Address ip_;
  Address ip_base_;
  unsigned tid_;

 public:
  UserThread() {}
  UserThread(Address ip, unsigned tid) :
      ip_(ip),
      tid_(tid),
      ip_base_("tcp://" + ip_ + ":") {}

  Address ip() const { return ip_; }

  unsigned tid() const { return tid_; }

  Address response_connect_address() const {
    return ip_base_ + std::to_string(tid_ + kUserResponsePort);
  }

  Address response_bind_address() const {
    return kBindBase + std::to_string(tid_ + kUserResponsePort);
  }

  Address key_address_connect_address() const {
    return ip_base_ + std::to_string(tid_ + kUserKeyAddressPort);
  }

  Address key_address_bind_address() const {
    return kBindBase + std::to_string(tid_ + kUserKeyAddressPort);
  }
};

#endif  // INCLUDE_ANNA_THREADS_HPP_
