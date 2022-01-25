#include "coord_handlers.hpp"

void query_handler(logger log, string &serialized, SocketCache &pushers,
                    map<Bucket, map<Key, set<Address>>> &bucket_key_address_map) {
  KeyQueryRequest query_request;
  query_request.ParseFromString(serialized);

  KeyQueryResponse query_response;
  query_response.set_response_id(query_request.request_id());

  vector<pair<BucketKey, GetAddressResult>> query_results;
  KVSError reported_error = KVSError::SUCCESS;

  for (const BucketKeyAddress &address : query_request.addresses()) {
    BucketKey bucket_key = get_bucket_key_from_addr(address);

    if (bucket_key_address_map.find(bucket_key.bucket_) != bucket_key_address_map.end()) {
      GetAddressResult get_result = get_address(bucket_key, bucket_key_address_map, log);
      if (get_result.error_ == KVSError::SUCCESS) {
        query_results.push_back(std::make_pair(bucket_key, get_result));
      }
      else {
        reported_error = get_result.error_;
        query_results.push_back(std::make_pair(bucket_key, get_result));
        break;
      }
    }
    else {
      reported_error = KVSError::BUCKET_NE;
      query_results.push_back(std::make_pair(bucket_key, GetAddressResult(reported_error)));
      break;
    }
  }

  for (const auto& pair : query_results) {
    BucketKeyAddress *addr_ptr = query_response.add_addresses();
    addr_ptr = get_addr_from_bucket_key(pair.first, addr_ptr);

    if (reported_error == KVSError::SUCCESS) {
      for (const Address& ip : pair.second.addr_){
        addr_ptr->add_ips(ip);
      }
    }
  }
  query_response.set_error(reported_error);

  query_results.clear();

  string resp_serialized;
  query_response.SerializeToString(&resp_serialized);

  kZmqUtil->send_string(resp_serialized, &pushers[query_request.response_address()]);
}
