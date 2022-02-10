#include "object_handlers.hpp"

extern map<string, unsigned> key_len_map;
extern map<string, string> key_val_map;

void copy_to_shm_obj(string &key_name, const char* data_src, unsigned data_size){
  auto shm_id = ipc::shm::acquire(key_name.c_str(), data_size);
  auto shm_ptr = static_cast<char*>(ipc::shm::get_mem(shm_id, nullptr));
  memcpy(shm_ptr, data_src, data_size);
}

pair<char*, unsigned> get_shm_obj(string &key_name){
  if (key_len_map.find(key_name) != key_len_map.end()){
    auto size_of_obj = key_len_map[key_name];
    auto shm_id = ipc::shm::acquire(key_name.c_str(), size_of_obj, ipc::shm::open);
    auto shm_ptr = static_cast<char*>(ipc::shm::get_mem(shm_id, nullptr));
    return std::make_pair(shm_ptr, size_of_obj);
  }
  else {
    return std::make_pair(nullptr, 0);
  }
} 

void check_object_arrival(logger log, BucketKey &bucket_key, map<Bucket, 
                          vector<TriggerPointer>> &bucket_triggers_map, vector<string> &active_triggers, 
                          vector<TriggerFunctionMetadata> &active_func_metadata){
  for (auto &trigger : bucket_triggers_map[bucket_key.bucket_]) {
    auto actions = trigger->action_for_new_object(bucket_key);
    int arg_flag = trigger->get_trigger_option() + 1;
    if (actions.size() > 0) {
      // log->info("Trigger function {} after BucketKey {} {} prepared", actions[0].function_, bucket_key.bucket_, bucket_key.key_);
      active_triggers.push_back(trigger->get_trigger_name());
      
      for (auto &action : actions){
        vector<string> func_args;
        bool cached_real_data = true;
        for (auto &session_key: action.session_keys_){
          string key_name = get_local_object_name(bucket_key.bucket_, session_key.second, session_key.first);
          if (key_val_map.find(key_name) == key_val_map.end()) {
            cached_real_data = false;
            break;
          }
        }
        if (cached_real_data) {
          arg_flag = 0;
          for (auto &session_key: action.session_keys_){
            string key_name = get_local_object_name(bucket_key.bucket_, session_key.second, session_key.first);
            func_args.push_back(key_val_map[key_name]);
          }
        }
        else {
          for (auto &session_key: action.session_keys_){
            string key_name = get_local_object_name(bucket_key.bucket_, session_key.second, session_key.first);
            func_args.push_back(key_name);
            func_args.push_back(std::to_string(key_len_map[key_name]));
          }
        }
        active_func_metadata.push_back({action.function_, func_args, arg_flag});
      }
    }
  }
}
