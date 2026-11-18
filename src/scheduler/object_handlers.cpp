#include "object_handlers.hpp"
#include "memory_trace.hpp"
#include <mutex>

extern map<string, unsigned> key_len_map;
extern map<string, string> key_val_map;

std::mutex object_mutex;
map<string, std::shared_ptr<char>> received_objects;

std::shared_ptr<char> map_shm_obj(const string &name, unsigned size, bool create) {
  auto id = ipc::shm::acquire(name.c_str(), size, create ? ipc::shm::create | ipc::shm::open : ipc::shm::open);
  if (id == nullptr) return {};
  return std::shared_ptr<char>(static_cast<char*>(ipc::shm::get_mem(id, nullptr)),
                              [id](char*) { ipc::shm::release(id); });
}

void set_shm_obj_size(const string &key_name, unsigned size) {
  std::lock_guard<std::mutex> lock(object_mutex);
  key_len_map[key_name] = size;
}

void release_shm_session(const string &session) {
  std::lock_guard<std::mutex> lock(object_mutex);
  for (auto it = key_len_map.begin(); it != key_len_map.end();) {
    if (it->first.compare(0, kvsKeyPrefix.size() + 1, kvsKeyPrefix + kDelimiter) != 0 &&
        is_session_key(it->first, session)) {
      ipc::shm::remove(it->first.c_str());
      received_objects.erase(it->first);
      if (key_val_map.erase(it->first)) {
        TRACE_MEMORY("cache_clear", session, it->first, &key_val_map, 0);
      }
      it = key_len_map.erase(it);
    }
    else ++it;
  }
}

void copy_to_shm_obj(string &key_name, const char* data_src, unsigned data_size){
  auto object = map_shm_obj(key_name, data_size, true);
  memcpy(object.get(), data_src, data_size);
  std::lock_guard<std::mutex> lock(object_mutex);
  received_objects[key_name] = std::move(object);
}

pair<std::shared_ptr<char>, unsigned> get_shm_obj(string &key_name){
  std::lock_guard<std::mutex> lock(object_mutex);
  if (key_len_map.find(key_name) != key_len_map.end()){
    auto size_of_obj = key_len_map[key_name];
    auto object = map_shm_obj(key_name, size_of_obj, false);
    if (object) return std::make_pair(std::move(object), size_of_obj);
  }
  return {nullptr, 0};
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
