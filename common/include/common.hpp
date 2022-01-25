#ifndef INCLUDE_COMMON_HPP_
#define INCLUDE_COMMON_HPP_

#include <algorithm>
#include <sstream>
#include "kvs.pb.h"
#include "types.hpp"

#include "anna.pb.h"
#include "lattices/lww_pair_lattice.hpp"
#include "lattices/multi_key_causal_lattice.hpp"
#include "lattices/priority_lattice.hpp"
#include "lattices/single_key_causal_lattice.hpp"
#include "zmq/socket_cache.hpp"
#include "zmq/zmq_util.hpp"

using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

const unsigned kMaxSocketNumber = 10000;

const string kDelimiter = "|";
const char kDelimiterChar = '|';

const unsigned msgDataPackingThreshold = 1024; // pack data into notification msg if data size < 1KB 

const string bucketNameDirectInvoc = "direct_invoc";
const string emptyString = string();
const string kvsKeyPrefix = "KVS";

inline string get_key_session_name(string key, string session){
  return key + session;
}

inline string get_local_object_name(string bucket, string key, string session) {
  return bucket + kDelimiter + get_key_session_name(key, session);
}

struct BucketKey {
  Bucket bucket_;
  Key key_;
  Session session_;

  BucketKey(){}

  BucketKey(Bucket &bucket, Key &key, Session &session): bucket_(bucket), key_(key), session_(session){}

  BucketKey(Bucket &bucket, string &session_key): bucket_(bucket) {
    session_ = "";
    auto key_size = session_key.size();
    if ( key_size > 16) {
      session_ = session_key.substr(key_size - 16);
      key_ = session_key.substr(0, key_size - 16);
    }
  }


  /**
   * return the key name used in shared memory
   */ 
  string shm_key_name() const {
    return get_local_object_name(bucket_, key_, session_);
  }

};

inline BucketKey get_bucket_key_from_string(string &key_name) {
  auto splitter_index = key_name.find(kDelimiter);
  string bucket = key_name.substr(0, splitter_index);
  string session_key = key_name.substr(splitter_index + 1);
  BucketKey bucket_key(bucket, session_key);
  return bucket_key;
}


inline BucketKeyTuple* get_tuple_from_bucket_key(const BucketKey &bucket_key, BucketKeyTuple *tp) {
  tp->set_bucket(bucket_key.bucket_);
  tp->set_key(bucket_key.key_);
  tp->set_session(bucket_key.session_);
  
  return tp;
}

inline BucketKey get_bucket_key_from_tuple(BucketKeyTuple &tuple) {
  BucketKey bucket_key;
  bucket_key.bucket_ = tuple.bucket();
  bucket_key.key_ = tuple.key();
  bucket_key.session_ = tuple.session();

  return bucket_key;
}

inline BucketKeyAddress* get_addr_from_bucket_key(const BucketKey &bucket_key, BucketKeyAddress *addr) {
  addr->set_bucket(bucket_key.bucket_);
  addr->set_key(bucket_key.key_);
  addr->set_session(bucket_key.session_);

  return addr;
}

inline BucketKey get_bucket_key_from_addr(const BucketKeyAddress &addr) {
  BucketKey bucket_key;
  bucket_key.bucket_ = addr.bucket();
  bucket_key.key_ = addr.key();
  bucket_key.session_ = addr.session();
  
  return bucket_key;
}

inline void split(const string& s, char delim, vector<string>& elems) {
  std::stringstream ss(s);
  string item;

  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
}

const char alpha_num[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
inline string gen_random(unsigned& seed, const int len) {
  string tmp_s;
  // unsigned seed = time(NULL);
  tmp_s.reserve(len);

  for (int i = 0; i < len; ++i) {
    tmp_s += alpha_num[rand_r(&seed) % (sizeof(alpha_num) - 1)];
  }
  return tmp_s;
}

// form the timestamp given a time and a thread id
inline unsigned long long get_time() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

inline unsigned long long generate_timestamp(const unsigned& id) {
  unsigned pow = 10;
  auto time = get_time();
  while (id >= pow) pow *= 10;
  return time * pow + id;
}

// Anna Lattices
// copy from anna common
inline string serialize(const LWWPairLattice<string>& l) {
  LWWValue lww_value;
  lww_value.set_timestamp(l.reveal().timestamp);
  lww_value.set_value(l.reveal().value);

  string serialized;
  lww_value.SerializeToString(&serialized);
  return serialized;
}

inline string serialize(const unsigned long long& timestamp,
                        const string& value) {
  LWWValue lww_value;
  lww_value.set_timestamp(timestamp);
  lww_value.set_value(value);

  string serialized;
  lww_value.SerializeToString(&serialized);
  return serialized;
}

inline string serialize(const SetLattice<string>& l) {
  SetValue set_value;
  for (const string& val : l.reveal()) {
    set_value.add_values(val);
  }

  string serialized;
  set_value.SerializeToString(&serialized);
  return serialized;
}

inline string serialize(const OrderedSetLattice<string>& l) {
  // We will just serialize ordered sets as regular sets for now;
  // order in serialization helps with performance
  // but is not necessary for correctness.
  SetValue set_value;
  for (const string& val : l.reveal()) {
    set_value.add_values(val);
  }

  string serialized;
  set_value.SerializeToString(&serialized);
  return serialized;
}

inline string serialize(const set<string>& set) {
  SetValue set_value;
  for (const string& val : set) {
    set_value.add_values(val);
  }

  string serialized;
  set_value.SerializeToString(&serialized);
  return serialized;
}

inline string serialize(const SingleKeyCausalLattice<SetLattice<string>>& l) {
  SingleKeyCausalValue sk_causal_value;
  auto ptr = sk_causal_value.mutable_vector_clock();
  // serialize vector clock
  for (const auto& pair : l.reveal().vector_clock.reveal()) {
    (*ptr)[pair.first] = pair.second.reveal();
  }
  // serialize values
  for (const string& val : l.reveal().value.reveal()) {
    sk_causal_value.add_values(val);
  }

  string serialized;
  sk_causal_value.SerializeToString(&serialized);
  return serialized;
}

inline string serialize(const MultiKeyCausalLattice<SetLattice<string>>& l) {
  MultiKeyCausalValue mk_causal_value;
  auto ptr = mk_causal_value.mutable_vector_clock();
  // serialize vector clock
  for (const auto& pair : l.reveal().vector_clock.reveal()) {
    (*ptr)[pair.first] = pair.second.reveal();
  }

  // serialize dependencies
  for (const auto& pair : l.reveal().dependencies.reveal()) {
    auto dep = mk_causal_value.add_dependencies();
    dep->set_key(pair.first);
    auto vc_ptr = dep->mutable_vector_clock();
    for (const auto& vc_pair : pair.second.reveal()) {
      (*vc_ptr)[vc_pair.first] = vc_pair.second.reveal();
    }
  }

  // serialize values
  for (const string& val : l.reveal().value.reveal()) {
    mk_causal_value.add_values(val);
  }

  string serialized;
  mk_causal_value.SerializeToString(&serialized);
  return serialized;
}

inline string serialize(const PriorityLattice<double, string>& l) {
  PriorityValue priority_value;
  priority_value.set_priority(l.reveal().priority);
  priority_value.set_value(l.reveal().value);

  string serialized;
  priority_value.SerializeToString(&serialized);
  return serialized;
}

inline LWWPairLattice<string> deserialize_lww(const string& serialized) {
  LWWValue lww;
  lww.ParseFromString(serialized);

  return LWWPairLattice<string>(
      TimestampValuePair<string>(lww.timestamp(), lww.value()));
}

inline SetLattice<string> deserialize_set(const string& serialized) {
  SetValue s;
  s.ParseFromString(serialized);

  set<string> result;

  for (const string& value : s.values()) {
    result.insert(value);
  }

  return SetLattice<string>(result);
}

inline OrderedSetLattice<string> deserialize_ordered_set(
    const string& serialized) {
  SetValue s;
  s.ParseFromString(serialized);
  ordered_set<string> result;
  for (const string& value : s.values()) {
    result.insert(value);
  }
  return OrderedSetLattice<string>(result);
}

inline SingleKeyCausalValue deserialize_causal(const string& serialized) {
  SingleKeyCausalValue causal;
  causal.ParseFromString(serialized);

  return causal;
}

inline MultiKeyCausalValue deserialize_multi_key_causal(
    const string& serialized) {
  MultiKeyCausalValue mk_causal;
  mk_causal.ParseFromString(serialized);

  return mk_causal;
}

inline PriorityLattice<double, string> deserialize_priority(
    const string& serialized) {
  PriorityValue pv;
  pv.ParseFromString(serialized);
  return PriorityLattice<double, string>(
      PriorityValuePair<double, string>(pv.priority(), pv.value()));
}

inline VectorClockValuePair<SetLattice<string>> to_vector_clock_value_pair(
    const SingleKeyCausalValue& cv) {
  VectorClockValuePair<SetLattice<string>> p;
  for (const auto& pair : cv.vector_clock()) {
    p.vector_clock.insert(pair.first, pair.second);
  }
  for (auto& val : cv.values()) {
    p.value.insert(std::move(val));
  }
  return p;
}

inline MultiKeyCausalPayload<SetLattice<string>> to_multi_key_causal_payload(
    const MultiKeyCausalValue& mkcv) {
  MultiKeyCausalPayload<SetLattice<string>> p;

  for (const auto& pair : mkcv.vector_clock()) {
    p.vector_clock.insert(pair.first, pair.second);
  }

  for (const auto& dep : mkcv.dependencies()) {
    VectorClock vc;
    for (const auto& pair : dep.vector_clock()) {
      vc.insert(pair.first, pair.second);
    }

    p.dependencies.insert(dep.key(), vc);
  }

  for (auto& val : mkcv.values()) {
    p.value.insert(std::move(val));
  }

  return p;
}

struct lattice_type_hash {
  std::size_t operator()(const LatticeType& lt) const {
    return std::hash<string>()(LatticeType_Name(lt));
  }
};

#endif  // INCLUDE_COMMON_HPP_
