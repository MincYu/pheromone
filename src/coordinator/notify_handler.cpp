#include "coord_handlers.hpp"
#include <sstream>
#include <iterator>

extern unsigned seed;
extern unsigned io_thread_num;

const int batchObjectThreshold = 10;
const char* batchDelim = "|";

std::hash<string> hasher;
/**
 * handle the notifying request from KVS.
 */ 
void notify_handler(logger log, string &serialized, SocketCache &pushers,
                    map<Bucket, map<Key, set<Address>>> &bucket_key_address_map,
                    map<Bucket, vector<TriggerPointer>> &bucket_triggers_map,
                    map<Bucket, string> &bucket_app_map,
                    map<string, string> &bucket_key_val_map,
                    map<string, set<Address>> &bucket_node_map,
                    map<Address, NodeStatus> &node_status_map) {
  auto receive_req_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

  KeyNotifyRequest key_notif_request;
  key_notif_request.ParseFromString(serialized);

  KeyNotifyResponse notif_resp;
  notif_resp.set_response_id(key_notif_request.request_id());

  set<string> active_triggers;
  for (const auto &trigger : key_notif_request.active_trigger_names()){
    active_triggers.insert(trigger);
  }

  KVSError kvs_error = KVSError::SUCCESS;
  // vector<pair<BucketKey, Address>> key_addr_pair;
  for (const BucketKeyAddress &address : key_notif_request.addresses()) {
    BucketKey bucket_key = get_bucket_key_from_addr(address);
    Address ip = address.ips(0);

    // check if it has the payload
    string payload = address.payload();
    if (!payload.empty()){
      bucket_node_map[bucket_key.bucket_].insert(ip.substr(6, ip.find(":", 6) - 6));
      update_address(bucket_key, ip, bucket_key_address_map, log);
      bucket_key_val_map[bucket_key.shm_key_name()] = payload;
    }
    else{
      // it is reasonable to assume there is only a single address (i.e., request sender)
      if (bucket_app_map.find(bucket_key.bucket_) != bucket_app_map.end()) {
        bucket_node_map[bucket_key.bucket_].insert(ip.substr(6, ip.find(":", 6) - 6));
        update_address(bucket_key, ip, bucket_key_address_map, log);
      }
      // it means the bucket has not been created
      else {
        kvs_error = KVSError::BUCKET_NE;
        log->info("Notifying handler error: Bucket not found");
        break;
      }
    }

    vector<string> triggered_name;
    // check trigger and scheduling
    for (auto &trigger : bucket_triggers_map[bucket_key.bucket_]) {
      auto actions = trigger->action_for_new_object(bucket_key);
      // skip the active trigger, which has already taken effect in local scheduler
      if (active_triggers.find(trigger->get_trigger_name()) != active_triggers.end()) {
        continue;
      }
      
      int arg_flag = trigger->get_trigger_option() + 1;
      if (actions.size() > 0) {
        // trigger function
        if (trigger->get_type() != PrimitiveType::BY_TIME) triggered_name.push_back(trigger->get_trigger_name());
        
        for (auto &action : actions) {
          auto trigger_stamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
          FunctionCall internalCall;
          internalCall.set_app_name(bucket_app_map[bucket_key.bucket_]);
          internalCall.set_resp_address(key_notif_request.response_address());
          internalCall.set_sync_data_status(true);
          internalCall.set_session_id(bucket_key.session_);
          auto req = internalCall.add_requests();
          req->set_name(action.function_);

          bool all_object = true;
          vector<string> val_to_batch;
          for (auto &session_key: action.session_keys_){
            string key_name = get_local_object_name(bucket_key.bucket_, session_key.second, session_key.first);
            if (bucket_key_val_map.find(key_name) != bucket_key_val_map.end()){
              val_to_batch.push_back(bucket_key_val_map[key_name]);
            }
            else{
              all_object = false;
              break;
            }
          }

          auto check_stamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

          // map<string, unsigned> data_locations_count_map;
          set<string> data_node;
          
          // shortcut for multiple small data
          if (all_object) {
            Argument* arg = req->add_arguments();

            std::ostringstream imploded;
            std::copy(val_to_batch.begin(), val_to_batch.end(), std::ostream_iterator<std::string>(imploded, batchDelim));

            arg->set_body(imploded.str());
            arg->set_arg_flag(0);
          }
          else {
            for (auto &session_key: action.session_keys_){
              Argument* arg = req->add_arguments();
              string key_name = get_local_object_name(bucket_key.bucket_, session_key.second, session_key.first);

              // if (bucket_key_val_map.find(key_name) != bucket_key_val_map.end()){
              //   arg->set_body(bucket_key_val_map[key_name]);
              //   arg->set_arg_flag(0);
              // }
              // else {
              arg->set_body(key_name);
              arg->set_arg_flag(arg_flag);
              // TODO session support
              string data_address = *(bucket_key_address_map[bucket_key.bucket_][session_key.second].begin());
              arg->set_data_address(data_address);
              string ip = data_address.substr(6, data_address.find(":", 6) - 6);
              data_node.insert(ip);
              // if (data_locations_count_map.find(ip) == data_locations_count_map.end()){
              //   data_locations_count_map[ip] = 1;
              // }
              // else{
              //   data_locations_count_map[ip]++;
              // }
              // }
            }
          }

          auto package_stamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

          string routed_worker;
          // // first we pick a node with the max number of objects if possible
          // vector<std::pair<string, unsigned>> data_locations_count_vector(data_locations_count_map.begin(), data_locations_count_map.end());
          // std::sort(data_locations_count_vector.begin(), data_locations_count_vector.end(), 
          //     [] (const auto& a, const auto& b) { return a.second > b.second || (a.second == b.second && hasher(a.first) >= hasher(b.first));});
          // for (auto ip_and_count: data_locations_count_vector){
          //   string ip = ip_and_count.first;
          //   if (node_status_map[ip].avail_executors_ > 0){
          //     routed_worker = ip;
          //     break;
          //   }
          // }

          // just pick the node with data and available slot
          int max_executors = 0;
          for (auto &ip : data_node){
            if (node_status_map[ip].avail_executors_ > max_executors){
              max_executors = node_status_map[ip].avail_executors_;
              routed_worker = ip;
            }
          }

          // if there is no available slot, we then pick other available node
          if (routed_worker.empty()){
            for (auto &ip_status : node_status_map) {
              if (ip_status.second.avail_executors_ > max_executors){
                max_executors = ip_status.second.avail_executors_;
                routed_worker = ip_status.first;
              }
            }
          }

          if (routed_worker.empty()){
            log->info("No available worker found for app {}", internalCall.app_name());
          }
          else{
            string address = get_func_exec_address(routed_worker, rand_r(&seed) % io_thread_num);
            auto scheduled_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            string call_serialized;
            internalCall.SerializeToString(&call_serialized);
            kZmqUtil->send_string(call_serialized, &pushers[address]);
            log->info("App data-driven call {} to {} arg_size {}. recv: {}, trigger: {}, check: {}, package: {}, scheduled {}.", 
                action.function_, routed_worker, action.session_keys_.size(), receive_req_stamp, trigger_stamp, check_stamp, package_stamp, scheduled_stamp);
            
            // update avail executors in advance
            node_status_map[routed_worker].avail_executors_--;
          }
        }
      }
    }

    // sync with nodes
    if (triggered_name.size() > 0){
      UpdateCoordMsg coord_msg;
      coord_msg.set_msg_type(1);

      for (auto &name : triggered_name) {
        TriggerEntity *entity = coord_msg.add_triggers();
        entity->set_bucket_name(bucket_key.bucket_);
        entity->set_trigger_name(name);
        entity->set_session(bucket_key.session_);
      }
      
      string msg_serialized;
      coord_msg.SerializeToString(&msg_serialized);
      for (auto &ip : bucket_node_map[bucket_key.bucket_]){
        string address = "tcp://" + ip + ":" + std::to_string(TriggerUpdatePort);
        kZmqUtil->send_string(msg_serialized, &pushers[address]);
      }
    }
  }
}
