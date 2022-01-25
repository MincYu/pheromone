#include "coord_handlers.hpp"

void app_register_handler(logger log, string &serialized, string &private_ip, unsigned &thread_id, SocketCache &pushers,
                        map<Bucket, vector<TriggerPointer>> &bucket_triggers_map,
                        map<string, set<string>> &app_buckets_map,
                        map<Bucket, string> &bucket_app_map,
                        map<Address, NodeStatus> &node_status_map) {
  AppRegistration appRegist;
  appRegist.ParseFromString(serialized);

  UpdateCoordMsg coord_msg;
  coord_msg.set_ip(private_ip);
  coord_msg.set_thread_id(thread_id);
  coord_msg.set_app_name(appRegist.app_name());
  coord_msg.set_msg_type(0);

  for (auto &func : appRegist.functions()){
    coord_msg.add_functions(func);
  }

  for (auto &dependency : appRegist.dependencies()){
    // keep the dependency if it is direct, otherwise create buckets & triggers
    if (dependency.type() == DependencyType::DIRECT){
      auto dep = coord_msg.add_dependencies();
      dep->CopyFrom(dependency);
    }
    else{
      string tgt_function = dependency.tgt_functions(0);
      string bucket_name = "b_" + tgt_function;
      app_buckets_map[appRegist.app_name()].insert(bucket_name);
      bucket_app_map[bucket_name] = appRegist.app_name();

      TriggerEntity *entity = coord_msg.add_triggers();
      entity->set_bucket_name(bucket_name);

      if (dependency.type() == DependencyType::MANY_TO_ONE){
        set<Key> key_names;
        for (auto &func : dependency.src_functions()) {
          key_names.insert("k_" + func);
        }
        string trigger_name = "t_" + std::to_string(DependencyType::MANY_TO_ONE) + "_" + tgt_function;
        auto trigger_ptr = std::make_shared<BySetTrigger>(tgt_function, trigger_name, key_names);
        bucket_triggers_map[bucket_name].push_back(trigger_ptr);
        
        entity->set_trigger_name(trigger_name);
        entity->set_trigger_option(trigger_ptr->get_trigger_option());
        entity->set_primitive_type(PrimitiveType::BY_SET);
        entity->set_primitive(trigger_ptr->dump_pritimive());
      }
      else if (dependency.type() == DependencyType::K_OUT_OF_N){
        vector<string> metadata;
        split(dependency.description(), ',', metadata);

        int k = stoi(metadata[0]);
        int n = stoi(metadata[1]);

        string trigger_name = "t_" + std::to_string(DependencyType::K_OUT_OF_N) + "_" + tgt_function;
        auto trigger_ptr = std::make_shared<RedundantTrigger>(tgt_function, trigger_name, k, n);
        bucket_triggers_map[bucket_name].push_back(trigger_ptr);

        entity->set_trigger_name(trigger_name);
        entity->set_trigger_option(trigger_ptr->get_trigger_option());
        entity->set_primitive_type(PrimitiveType::REDUNDANT);
        entity->set_primitive(trigger_ptr->dump_pritimive());
      }
      else if (dependency.type() == DependencyType::PERIODIC){
        string tgt_function = dependency.tgt_functions(0);
        string bucket_name = "b_" + tgt_function;
        app_buckets_map[appRegist.app_name()].insert(bucket_name);
        bucket_app_map[bucket_name] = appRegist.app_name();

        int time_window = stoi(dependency.description());

        string trigger_name = "t_" + std::to_string(DependencyType::PERIODIC) + "_" + tgt_function;
        auto trigger_ptr = std::make_shared<ByTimeTrigger>(tgt_function, trigger_name, time_window);
        bucket_triggers_map[bucket_name].push_back(trigger_ptr);
      }
    }
  }

  string msg_serialized;
  coord_msg.SerializeToString(&msg_serialized);
  for (auto &ip_status : node_status_map) {
    string address = "tcp://" + ip_status.first + ":" + std::to_string(TriggerUpdatePort);
    kZmqUtil->send_string(msg_serialized, &pushers[address]);
  }
  log->info("Application registered. name: {}", appRegist.app_name());
}