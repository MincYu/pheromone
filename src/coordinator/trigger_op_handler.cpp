#include "coord_handlers.hpp"

void trigger_op_handler(logger log, string &serialized, string &private_ip, unsigned &thread_id, SocketCache &pushers,
                        map<Bucket, vector<TriggerPointer>> &bucket_triggers_map,
                        map<Bucket, string> &bucket_app_map,
                        map<Address, NodeStatus> &node_status_map){
  TriggerOperationRequest request;
  request.ParseFromString(serialized);

  TriggerOperationResponse response;
  response.set_response_id(request.request_id());

  Bucket bucket_name = request.bucket_name();
  response.set_bucket_name(bucket_name);

  string trigger_name = request.trigger_name();
  response.set_trigger_name(trigger_name);

  // Update triggers to local schedulers
  UpdateCoordMsg trigger_msg;
  trigger_msg.set_ip(private_ip);
  trigger_msg.set_thread_id(thread_id);
  trigger_msg.set_app_name(request.app_name());

  if (request.operation_type() == TriggerOperationType::ADD_TRIGGER) {
    if (bucket_app_map.find(bucket_name) != bucket_app_map.end()) {
      auto trigger_ptr = gen_trigger_pointer(request.primitive_type(), trigger_name, request.primitive());
      if (trigger_ptr != nullptr){
        trigger_ptr->set_trigger_option(request.trigger_option());
        bucket_triggers_map[bucket_name].push_back(trigger_ptr);
      }
      else {
        log->error("Unknown primitive type when adding Trigger {} in Bucket {}.", trigger_name, bucket_name);
      }
      response.set_error(KVSError::SUCCESS);
      log->info("Add Trigger {} in Bucket {}.", trigger_name, bucket_name);

      trigger_msg.set_msg_type(0);

      if (trigger_ptr->get_type() != PrimitiveType::BY_TIME) {
        TriggerEntity *entity = trigger_msg.add_triggers();
        entity->set_bucket_name(bucket_name);
        entity->set_trigger_name(trigger_name);
        entity->set_trigger_option(request.trigger_option());
        entity->set_primitive_type(request.primitive_type());
        entity->set_primitive(trigger_ptr->dump_pritimive());
        entity->mutable_hints()->CopyFrom(request.hints()); 
      }
    }
    // it means the bucket does not exist, so we just return
    else {
      response.set_error(KVSError::BUCKET_NE);
    }
  }
  else if (request.operation_type() == TriggerOperationType::DELETE_TRIGGER) {
    if (bucket_app_map.find(bucket_name) != bucket_app_map.end()) {
      bool find_trigger = false;

      for (auto it = bucket_triggers_map[bucket_name].begin(); it != bucket_triggers_map[bucket_name].end();) {
        TriggerPointer ptr = *it;
        if (ptr->get_trigger_name() == trigger_name) {
          it = bucket_triggers_map[bucket_name].erase(it);
          find_trigger = true;
        }
        else {
          it++;
        }
      }
      if (find_trigger) {
        trigger_msg.set_msg_type(2);
        TriggerEntity *entity = trigger_msg.add_triggers();
        entity->set_bucket_name(bucket_name);
        entity->set_trigger_name(trigger_name);
        
        response.set_error(KVSError::SUCCESS);
        log->info("Delete Trigger {} in Bucket {}.", trigger_name, bucket_name);
      }
      else {
        log->error("Trigger is not found when deleting Trigger {} in Bucket {}.", trigger_name, bucket_name);
      }
    }
    else {
      response.set_error(KVSError::BUCKET_NE);
    }
  }
  // unknow type
  else {
    log->error("Unknown operation type for Trigger {} in Bucket {}.", trigger_name, bucket_name);
  }

  string resp_serialized;
  response.SerializeToString(&resp_serialized);
  kZmqUtil->send_string(resp_serialized, &pushers[request.response_address()]);

  string trigger_serialized;
  trigger_msg.SerializeToString(&trigger_serialized);

  for (auto &ip_status : node_status_map) {
    string address = "tcp://" + ip_status.first + ":" + std::to_string(TriggerUpdatePort);
    kZmqUtil->send_string(trigger_serialized, &pushers[address]);
  }
}
