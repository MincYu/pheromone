#include "coord_handlers.hpp"

void bucket_op_handler(logger log, string &serialized, SocketCache &pushers,
                        map<Bucket, vector<TriggerPointer>> &bucket_triggers_map,
                        map<string, set<string>> &app_buckets_map,
                        map<Bucket, string> &bucket_app_map) {
  BucketOperationRequest request;
  request.ParseFromString(serialized);

  BucketOperationResponse response;
  response.set_response_id(request.request_id());

  Bucket bucket_name = request.bucket_name();
  response.set_bucket_name(bucket_name);

  string app_name = request.app_name();

  if (request.operation_type() == BucketOperationType::CREATE_BUCKET) {
    // it means the bucket has been created, so we return the error
    if (bucket_app_map.find(bucket_name) != bucket_app_map.end()) {
      response.set_error(KVSError::BUCKET_EXIST);
    }
    else {
      app_buckets_map[app_name].insert(bucket_name);
      bucket_app_map[bucket_name] = app_name;
      response.set_error(KVSError::SUCCESS);
      log->info("Create Bucket {}.", bucket_name);
    }
  }
  else if (request.operation_type() == BucketOperationType::DELETE_BUCKET) {
    // it means the bucket does not exist, so we return the error
    if (bucket_app_map.find(bucket_name) == bucket_app_map.end()) {
      response.set_error(KVSError::BUCKET_NE);
    }
    else {
      // TODO delete address map and notify corresponding KVS
      app_buckets_map[request.app_name()].erase(bucket_name);
      bucket_app_map.erase(bucket_name);
      if (bucket_triggers_map.find(bucket_name) != bucket_triggers_map.end()) {
        bucket_triggers_map.erase(bucket_name);
      }
      response.set_error(KVSError::SUCCESS);
      log->info("Delete Bucket {}.", bucket_name);
    }
  }
  // unknow type
  else {
    log->error("Unknown operation type for Bucket {}.", bucket_name);
  }

  string resp_serialized;
  response.SerializeToString(&resp_serialized);

  kZmqUtil->send_string(resp_serialized, &pushers[request.response_address()]);
}