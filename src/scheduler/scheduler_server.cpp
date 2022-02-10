#include "object_handlers.hpp"
#include "comm_helper.hpp"
#include "anna_client/kvs_client.hpp"
#include "yaml-cpp/yaml.h"

using shm_chan_t = ipc::chan<ipc::relat::multi, ipc::relat::multi, ipc::trans::unicast>;

// using namespace ipc::shm;

struct InflightIORequest {
  uint8_t executor_id_;
  uint8_t req_id_;
  TimePoint recv_time_;
};

struct DelayFunctionCall {
  string session_id_;
  string app_name_;
  string func_name_;
  vector<string> func_args_;
  int arg_flag_;
  TimePoint trigger_time_;
};

constexpr char const name__  [] = "ipc-kvs";
shm_chan_t shared_chan { name__, ipc::receiver };

map<uint8_t, shm_chan_t*> executor_chans_map;
std::atomic<bool> is_quit__{ false };

unsigned forwardDataPackingThreshold = 10240; // pack data into request if data size < 10KB 
unsigned schedTimerThreshold = 10; // every 10 ms
unsigned ramSeed = time(NULL);
string funcDir;
unsigned schedDelayTime; // delay time in us
bool sharedExecutor;  // we can configure whether to employ shared settings
bool rejectExtraReq;

queue<DelayFunctionCall> delay_call_queue;

map<string, unsigned> key_len_map;
map<string, string> key_val_map;
map<string, string> session_client_addr_map;

struct RerunCheckQueueItem {
  TimePoint check_time_; // checking timestamp in ms
  string session_;
  TriggerPointer trigger_ptr_;
};

auto rerun_check_queue_compare = [](RerunCheckQueueItem first, RerunCheckQueueItem second) {return first.check_time_ > second.check_time_;};
using RerunCheckQueue = std::priority_queue<RerunCheckQueueItem, vector<RerunCheckQueueItem>, decltype(rerun_check_queue_compare)>;

RerunCheckQueue rerun_check_queue(rerun_check_queue_compare);
map<string, vector<RerunTriggerTimeout>> func_trigger_timeout_map;

inline int get_avail_executor_num(map<uint8_t, uint8_t> &executor_status_map){
  int executors = 0;
  for (auto &executor_status : executor_status_map) {
    if (executor_status.second == 0 || executor_status.second == 1) executors++;
    // if (sharedExecutor) {
    //   if (executor_status.second == 0 || executor_status.second == 1) executors++;
    // }
    // else{
    //   // TODO different coordinators have different views based on the functions they manage
    //   if (executor_status.second == 0) executors++;
    // }
  }
  return executors;
}

inline void send_to_executer(shm_chan_t* chan_ptr, string msg){
  if (!chan_ptr->send(msg)) {
    std::cerr << "Send to executer failed.\n";

    if (!chan_ptr->wait_for_recv(1)) {
        std::cerr << "Wait receiver failed.\n";
    }
  }
}

map<string, uint8_t> data_clear_state_map; // state 0: created; 1: be using; 2: ready to clear
inline void release_shm_object() {
  // TODO clear the shared memory
}

void forward_call_via_helper(CommHelperInterface *helper, string &session_id, string &app_name, vector<string> &func_name_vec, 
                            vector<vector<string>> &func_args_vec, int arg_flag){
  // send request to global coordinator
  if (arg_flag > 0){
    int total_data_size = 0;
    for (auto &func_args : func_args_vec){
      for (int i = 0; i < func_args.size(); i+=3){
        total_data_size +=  stoi(func_args[i + 2]);
      }
    }
    if (total_data_size < forwardDataPackingThreshold){
      vector<vector<string>> data_args_vec;
      for (auto &func_args : func_args_vec){
        vector<string> real_data_args;
        for (int i = 0; i < func_args.size(); i+=3){
          string key_name = func_args[i] + kDelimiter + func_args[i + 1];
          auto size_len = key_len_map[key_name];
          auto shm_id = ipc::shm::acquire(key_name.c_str(), size_len, ipc::shm::open);
          auto shm_ptr = static_cast<char*>(ipc::shm::get_mem(shm_id, nullptr));
          string real_data(shm_ptr, size_len);
          if (arg_flag == 1) {
            real_data_args.push_back(real_data);
          }
          else if (arg_flag == 2) {
            real_data_args.push_back(func_args[i + 1]);
            real_data_args.push_back(real_data);
          }
        }
        data_args_vec.push_back(real_data_args);
      }

      helper->forward_func_call(session_client_addr_map[session_id], app_name, func_name_vec, data_args_vec, 0, session_id);
    }
    else{
      helper->forward_func_call(session_client_addr_map[session_id], app_name, func_name_vec, func_args_vec, arg_flag, session_id);
    }
  }
  else{
    helper->forward_func_call(session_client_addr_map[session_id], app_name, func_name_vec, func_args_vec, arg_flag, session_id);
  }
}

void schedule_func_call(logger log, CommHelperInterface *helper, map<uint8_t, uint8_t> &executor_status_map, map<string, set<uint8_t>> &function_executor_map,
                        string &session_id, string &app_name, string &func_name, vector<string> &func_args, int arg_flag){
  vector<uint8_t> avail_warm_executors;
  vector<uint8_t> avail_cold_executors;
  for (auto &executor_status : executor_status_map) {
    if (executor_status.second == 1 && function_executor_map[func_name].find(executor_status.first) != function_executor_map[func_name].end()){
      // find an available warm node, just move forward
      avail_warm_executors.push_back(executor_status.first);
      break;
    }
    if (executor_status.second == 0 || (sharedExecutor && executor_status.second == 1)){
      avail_cold_executors.push_back(executor_status.first);
    }
  }
  if (avail_warm_executors.size() > 0 || avail_cold_executors.size() > 0){
    auto executor_id = 0;
    if (avail_warm_executors.size() > 0){
      // randomly pick a warm executor
      executor_id = avail_warm_executors[rand_r(&ramSeed) % avail_warm_executors.size()];
    }
    else {
      // randomly pick a cold executor
      executor_id = avail_cold_executors[rand_r(&ramSeed) % avail_cold_executors.size()];
      function_executor_map[func_name].insert(executor_id);
    }

    // 1 means function call
    string resp;
    resp.push_back(1);
    // arg_flag. 0:actual value; 1:key value; 2:need both key name and key value
    resp.push_back(static_cast<uint8_t>(arg_flag + 1));
    // persist_output_flag. 1: persist; 2: not
    resp.push_back(static_cast<uint8_t>(session_client_addr_map[session_id].empty() ? 1 : 2));
    resp += session_id + "|";

    string func_args_string(func_name);
    for (int i = 0; i < func_args.size(); i++){
      func_args_string = func_args_string + "|" + func_args[i];
    }
    resp += func_args_string;
    
    auto sched_time = std::chrono::system_clock::now();
    auto sched_stamp = std::chrono::duration_cast<std::chrono::microseconds>(sched_time.time_since_epoch()).count();

    // std::cout << "Schedule function " << func_name << " to executor " << executor_id << "\n" << std::flush;
    log->info("Schedule function {} with arg_flag {} to executor {}. sched_time: {}", func_name, arg_flag, executor_id, sched_stamp);
    send_to_executer(executor_chans_map[executor_id], resp);
    executor_status_map[executor_id] = 2;

    // update rerun check queue
    for (auto &func_rerun_meta: func_trigger_timeout_map[func_name]){
      func_rerun_meta.trigger_ptr_->notify_source_func(func_name, session_id, func_args, arg_flag);
      auto check_time = sched_time + std::chrono::milliseconds(func_rerun_meta.timeout_);
      rerun_check_queue.push({check_time, session_id, func_rerun_meta.trigger_ptr_});
    }
  }
  else{
    // log->info("No local executor available in scheduling.");
    if (schedDelayTime > 0){
      delay_call_queue.push(DelayFunctionCall{session_id, app_name, func_name, func_args, arg_flag, std::chrono::system_clock::now()});
    }
    else {
      if (!rejectExtraReq) {
        vector<string> func_name_vec;
        func_name_vec.push_back(func_name);
        vector<vector<string>> func_args_vec;
        if (arg_flag > 0 && func_args[0].find('|') != std::string::npos) {
          vector<string> flatten_args;
          for (auto arg : func_args){
            if (arg.find('|') != std::string::npos) {
              vector<string> splits;
              split(arg, '|', splits);
              for (auto &s : splits) flatten_args.push_back(s);
            }
            else{
              flatten_args.push_back(arg);
            }
          }
          func_args_vec.push_back(flatten_args);
        }
        else{
          func_args_vec.push_back(func_args);
        }
        forward_call_via_helper(helper, session_id, app_name, func_name_vec, func_args_vec, arg_flag);
      }
    }
  }
}

/**
 * Return triggers employed
 */ 
vector<string> check_trigger(logger log, BucketKey &bucket_key, string &session_id, 
                            CommHelperInterface *helper, map<Bucket, vector<TriggerPointer>> &bucket_triggers_map, map<string, set<uint8_t>> &function_executor_map,
                            map<uint8_t, uint8_t> &executor_status_map, map<string, string> &bucket_app_map) {
  vector<string> active_triggers;
  vector<TriggerFunctionMetadata> active_func_metadata;
  check_object_arrival(log, bucket_key, bucket_triggers_map, active_triggers, active_func_metadata);

  // if (active_func_metadata.size() > 0){
  //   log->info("Trigger {} function(s) when BucketKey {} {} arrives", active_func_metadata.size(), bucket_key.bucket_, bucket_key.key_);
  // }
  for (auto &func_metadata : active_func_metadata) {
    schedule_func_call(log, helper, executor_status_map, function_executor_map, session_id, 
                        bucket_app_map[bucket_key.bucket_], func_metadata.func_name_, func_metadata.func_args_, func_metadata.arg_flag_);
  }
  
  return active_triggers;
}


void run(CommHelperInterface *helper, Address ip, unsigned thread_id, unsigned executor) {
  string log_file = "log_scheduler_" + std::to_string(thread_id) + ".txt";
  string log_name = "log_scheduler_" + std::to_string(thread_id);
  auto log = spdlog::basic_logger_mt(log_name, log_file, true);
  log->set_pattern("[%Y-%m-%d %T.%f] [thread %t] [%l] %v");
  log->flush_on(spdlog::level::info);
  helper->set_logger(log);
  for (auto kvs_client : kvs_clients){
    kvs_client->set_logger(log);
  }
  
  unsigned call_id = 0;
  
  map<unsigned, InflightFuncArgs> call_id_inflight_args_map;
  map<string, set<unsigned>> key_call_id_map;
  map<string, vector<InflightIORequest>> key_remote_get_map; // both ephemeral and durable data
  map<string, vector<InflightIORequest>> key_ksv_put_map;

  map<string, AppInfo> app_info_map;
  map<string, string> func_app_map;
  map<string, string> bucket_app_map;

  set<string> function_cache;
  map<string, uint32_t> function_len_map;
  map<string, set<uint8_t>> function_executor_map;
  
  map<uint8_t, uint8_t> executor_status_map;
  for (uint8_t e_id = 0; e_id < executor; e_id++){
    // 0: empty, 1: available, 2: await, 3: busy, 4: error
    executor_status_map[e_id] = 2;
    
    string chan_name = "ipc-" + std::to_string(e_id);
    shm_chan_t * executor_chan_ = new shm_chan_t{chan_name.c_str(), ipc::sender};
    executor_chans_map[e_id] = executor_chan_;

    if(!executor_chans_map[e_id]->reconnect(ipc::sender)){
      std::cerr << "Reconnect executor chan " << e_id << " failed" << std::endl;
    }
  }
  
  map<Bucket, vector<TriggerPointer>> bucket_triggers_map;

  std::cout << "Running kvs server...\n";
  log->info("Running kvs server");

  auto report_start = std::chrono::system_clock::now();
  auto report_end = std::chrono::system_clock::now();

  while (true){
    // TODO timeout
    auto dd = shared_chan.recv(0);
    auto str = static_cast<char*>(dd.data());

    if (str != nullptr) {
      auto recv_stamp = std::chrono::system_clock::now();

      // executor address (1 byte) | requst type (1 byte) | request id (1 byte) | metadata len (1 byte)| metadata | optional value
      auto executor_address = str[0];
      uint8_t executor_id = static_cast<uint8_t>(executor_address - 1);


      // get request
      if (str[1] == 1){
        bool from_ephe_store = static_cast<uint8_t>(str[2]) == 1;
        auto req_id = static_cast<uint8_t>(str[3]);
        auto meta_data_len = static_cast<uint8_t>(str[4]);

        string key_name(str + 5, meta_data_len);

        if(from_ephe_store){
          if (key_len_map.find(key_name) != key_len_map.end()) {
            string resp;
            // msg type | request id (1 byte) | is_success (1 byte) | optional value
            // 3 means generic response
            resp.push_back(3);
            resp.push_back(req_id);
            auto size_len = key_len_map[key_name];

            resp.push_back(1);
            resp += std::to_string(size_len);

            auto ready_stamp = std::chrono::system_clock::now();
            auto ready_time = std::chrono::duration_cast<std::chrono::microseconds>(ready_stamp.time_since_epoch()).count();
            auto recv_time = std::chrono::duration_cast<std::chrono::microseconds>(recv_stamp.time_since_epoch()).count();

            log->info("Get local {}, recv: {}, ready: {}", key_name, recv_time, ready_time);

            send_to_executer(executor_chans_map[executor_id], resp);
          }
          else {
            // std::cout << key_name << " not exists\n";
            helper->send_data_req(key_name);
            InflightIORequest get_req = {executor_id, req_id, recv_stamp};
            key_remote_get_map[key_name].push_back(get_req);
          }
        }
        else {
          vector<string> key_info;
          split(key_name, '|', key_info);
          helper->get_kvs_async(key_info[1]);
          InflightIORequest get_req = {executor_id, req_id, recv_stamp};
          key_remote_get_map[key_name].push_back(get_req);
        }
      }

      // send object handler
      if (str[1] >= 2 && str[1] <= 4){
        auto req_id = static_cast<uint8_t>(str[2]);
        auto is_data_packing = static_cast<uint8_t>(str[3]);

        string params(str + 4);
        vector<string> infos;
        split(params, '|', infos);

        if (infos.size() < 6) {
          log->warn("Send object handler has no enough argument {} params {}", infos.size(), params);
        }
        else{
          // unpacking infos
          string& session_id = infos[0], src_function = infos[1], tgt_function = infos[2], bucket = infos[3], session_key = infos[4];
          string obj_name = bucket + kDelimiter + session_key;

          // ephemeral data
          if (str[1] == 2) {
            if (is_data_packing == 1) {
              key_val_map[obj_name] = infos[5];
              key_len_map[obj_name] = key_val_map[obj_name].size();
            }
            else if (is_data_packing == 2){
              int size_int = stoi(infos[5]);
              key_len_map[obj_name] = size_int;
            }
            
            string app_name = func_app_map[src_function];
            // try direct invocation
            if (app_info_map[app_name].direct_deps_.find(src_function) != app_info_map[app_name].direct_deps_.end()){

              vector<string> func_args;
              int arg_flag = 1;
              if (is_data_packing == 1) {
                func_args.push_back(key_val_map[obj_name]);
                arg_flag = 0;
              }
              else if (is_data_packing == 2){
                func_args = {obj_name, infos[5]};
              }

              if (tgt_function.empty()){
                // batch forwading
                vector<string> target_funcs;
                for (auto target_dep : app_info_map[app_name].direct_deps_[src_function]) target_funcs.push_back(target_dep);
                
                auto avail_executors = get_avail_executor_num(executor_status_map);
                if (target_funcs.size() > avail_executors){
                  vector<string> local_funcs(target_funcs.begin(), target_funcs.begin() + avail_executors);
                  vector<string> remote_funcs(target_funcs.begin() + avail_executors, target_funcs.end());
                  for (auto &func : local_funcs) schedule_func_call(log, helper, executor_status_map, function_executor_map, session_id, app_name, func, func_args, arg_flag);
                  if (!rejectExtraReq){
                    vector<vector<string>> func_args_vec;
                    for (int i = 0; i < remote_funcs.size(); i++) func_args_vec.push_back(func_args);
                    forward_call_via_helper(helper, session_id, app_name, remote_funcs, func_args_vec, arg_flag);
                  }
                }
                else{
                  for (auto &func : target_funcs) schedule_func_call(log, helper, executor_status_map, function_executor_map, session_id, app_name, func, func_args, arg_flag);
                }
              }
              else if (app_info_map[app_name].direct_deps_[src_function].find(tgt_function) != app_info_map[app_name].direct_deps_[src_function].end()) {
                schedule_func_call(log, helper, executor_status_map, function_executor_map, session_id, app_name, tgt_function, func_args, arg_flag);
              }
              else {
                log->warn("Direct Invocation without pre-defined dependency {} -> {}", src_function, tgt_function);
              }
            }
            // try trigger check
            else if (bucket != bucketNameDirectInvoc){
              BucketKey bucket_key = BucketKey(bucket, session_key);
              auto active_triggers = check_trigger(log, bucket_key, session_id, helper, bucket_triggers_map, function_executor_map, executor_status_map, bucket_app_map);
              if (is_data_packing == 1) {
                helper->notify_put(bucket_key, active_triggers, session_client_addr_map[session_id], key_val_map[obj_name]);
              }
              else if (is_data_packing == 2){
                helper->notify_put(bucket_key, active_triggers, session_client_addr_map[session_id]);
              }
              log->info("Notified data {}", obj_name);
              call_id += active_triggers.size();
            }
          }
          // to remote data store
          else if (str[1] == 3) {
            // currently we use anna
            if (is_data_packing == 1) {
              string output_data = infos[5];
              char *val_ptr = new char[output_data.size()];
              strcpy(val_ptr, output_data.c_str());
              helper->put_kvs_async(session_key, val_ptr, output_data.size());
            }
            else if (is_data_packing == 2){
              int size_int = stoi(infos[5]);
              auto shm_id = ipc::shm::acquire(obj_name.c_str(), size_int, ipc::shm::open);
              auto shm_ptr = static_cast<char*>(ipc::shm::get_mem(shm_id, nullptr));
              helper->put_kvs_async(session_key, shm_ptr, size_int);
            }

            InflightIORequest put_req = {executor_id, req_id, recv_stamp};
            key_ksv_put_map[session_key].push_back(put_req);
          }
          // response to client
          else if (str[1] == 4) {
            string output_data;
            if (is_data_packing == 1) {
              output_data = infos[5];
            }
            else if (is_data_packing == 2){
              int size_int = stoi(infos[5]);
              auto shm_id = ipc::shm::acquire(obj_name.c_str(), size_int, ipc::shm::open);
              auto shm_ptr = static_cast<char*>(ipc::shm::get_mem(shm_id, nullptr));
              output_data = string(shm_ptr, size_int);
            }

            helper->client_response(session_client_addr_map[session_id], func_app_map[src_function], output_data);
            // TODO remove output data
          }
        }
      }

      // executor busy flag
      if (str[1] == 6){
        executor_status_map[executor_id] = 3;
        // helper->update_status(get_avail_executor_num(executor_status_map), function_cache);
      }

      // executor available flag
      if (str[1] == 7){
        if (executor_status_map[executor_id] == 0 || executor_status_map[executor_id] == 2){
          executor_status_map[executor_id] = 0;
        }
        else{
          executor_status_map[executor_id] = 1;
        }
        // helper->update_status(get_avail_executor_num(executor_status_map), function_cache);
      }
    }

    // handle message from comm_helper
    auto comm_resps = helper->try_recv_msg(bucket_triggers_map, app_info_map, func_app_map, bucket_app_map, func_trigger_timeout_map);
    for (auto &comm_resp : comm_resps) {
      if (comm_resp.msg_type_ == RecvMsgType::Call) {
        session_client_addr_map[comm_resp.session_id_] = comm_resp.resp_address_;

        for (int i = 0; i < comm_resp.func_name_.size(); i++){
          auto func_name = comm_resp.func_name_[i];
          auto func_args = comm_resp.func_args_[i];
          auto is_func_arg_key = comm_resp.is_func_arg_keys_[i];
          if (is_func_arg_key > 0) {
            InflightFuncArgs inflight_args;
            inflight_args.func_name_ = func_name;
            inflight_args.session_id_ = comm_resp.session_id_;
            inflight_args.arg_flag_ = is_func_arg_key;
            for (auto &arg : func_args){
              inflight_args.key_args_.push_back(arg);
              if (key_len_map.find(arg) != key_len_map.end()){
                continue;
              }
              inflight_args.inflight_args_.insert(arg);
              // we only send the data query if there is no inflight one
              if (key_call_id_map.find(arg) == key_call_id_map.end()){
                helper->send_data_req(arg);
              }
              key_call_id_map[arg].insert(call_id);
            }
            auto check_name = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
            log->info("Check and fetch function args at: {}", check_name);
            // If all the args can be found locally, we just schedule this call
            if (inflight_args.inflight_args_.empty()){
              vector<string> func_full_args;
              for (auto &key : inflight_args.key_args_){
                func_full_args.push_back(key);
                func_full_args.push_back(std::to_string(key_len_map[key]));
              }
              schedule_func_call(log, helper, executor_status_map, function_executor_map, comm_resp.session_id_, comm_resp.app_name_, inflight_args.func_name_, func_full_args, is_func_arg_key);
            }
            else{
              call_id_inflight_args_map[call_id] = inflight_args;
            }
          }
          else{
            schedule_func_call(log, helper, executor_status_map, function_executor_map, comm_resp.session_id_, comm_resp.app_name_, func_name, func_args, 0);
          }
          call_id++;
        }
      }
      else if (comm_resp.msg_type_ == RecvMsgType::DataResp){
        key_len_map[comm_resp.data_key_] = comm_resp.data_size_;
        // check if there is any waiting get request
        if (key_remote_get_map.find(comm_resp.data_key_) != key_remote_get_map.end()){
          for (auto &inflight_get_req : key_remote_get_map[comm_resp.data_key_]){
            string resp;
            resp.push_back(3);
            resp.push_back(inflight_get_req.req_id_);
            resp.push_back(1);
            resp += comm_resp.data_size_;

            auto recv_stamp = std::chrono::duration_cast<std::chrono::microseconds>(inflight_get_req.recv_time_.time_since_epoch()).count();
            auto cur_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
            log->info("Get remote {}, recv: {}, ready: {}", comm_resp.data_key_, recv_stamp, cur_stamp);
            send_to_executer(executor_chans_map[inflight_get_req.executor_id_], resp);
          }
          key_remote_get_map.erase(comm_resp.data_key_);
        }

        // check if there is any waiting function
        if (key_call_id_map.find(comm_resp.data_key_) != key_call_id_map.end()){
          // schedule function if possible
          for (auto cid : key_call_id_map[comm_resp.data_key_]){
            call_id_inflight_args_map[cid].inflight_args_.erase(comm_resp.data_key_);
            if (call_id_inflight_args_map[cid].inflight_args_.empty()){
              vector<string> func_args;
              for (auto &key : call_id_inflight_args_map[cid].key_args_){
                func_args.push_back(key);
                func_args.push_back(std::to_string(key_len_map[key]));
              }
              schedule_func_call(log, helper, executor_status_map, function_executor_map,
                                call_id_inflight_args_map[cid].session_id_,
                                func_app_map[call_id_inflight_args_map[cid].func_name_],
                                call_id_inflight_args_map[cid].func_name_, 
                                func_args, call_id_inflight_args_map[cid].arg_flag_);
              call_id_inflight_args_map.erase(cid);
            }
          }
          key_call_id_map.erase(comm_resp.data_key_);
        }

      }
      else if (comm_resp.msg_type_ == RecvMsgType::KvsGetResp){
        string local_obj_name = kvsKeyPrefix + "|" + comm_resp.data_key_;
        key_len_map[local_obj_name] = comm_resp.data_size_;
        // TODO clear kvs object

        if (key_remote_get_map.find(local_obj_name) != key_remote_get_map.end()){
          for (auto &inflight_get_req : key_remote_get_map[local_obj_name]){
            string resp;
            resp.push_back(3);
            resp.push_back(inflight_get_req.req_id_);
            resp.push_back(1);
            resp += std::to_string(comm_resp.data_size_);

            auto recv_stamp = std::chrono::duration_cast<std::chrono::microseconds>(inflight_get_req.recv_time_.time_since_epoch()).count();
            auto cur_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
            log->info("Kvs GET {}, recv: {}, ready: {}", comm_resp.data_key_, recv_stamp, cur_stamp);
            send_to_executer(executor_chans_map[inflight_get_req.executor_id_], resp);
          }
          key_remote_get_map.erase(local_obj_name);
        }
      }
      else if (comm_resp.msg_type_ == RecvMsgType::KvsPutResp){
        if (key_ksv_put_map.find(comm_resp.data_key_) != key_ksv_put_map.end()){
          for (auto &inflight_put_req : key_ksv_put_map[comm_resp.data_key_]){
            string resp;
            resp.push_back(3);
            resp.push_back(inflight_put_req.req_id_);
            resp.push_back(1);

            auto recv_stamp = std::chrono::duration_cast<std::chrono::microseconds>(inflight_put_req.recv_time_.time_since_epoch()).count();
            auto cur_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
            log->info("Kvs PUT {}, recv: {}, ready: {}", comm_resp.data_key_, recv_stamp, cur_stamp);
            send_to_executer(executor_chans_map[inflight_put_req.executor_id_], resp);
          }
          key_ksv_put_map.erase(comm_resp.data_key_);
        }
      }
    }

    // delay forwarding
    if (!delay_call_queue.empty()){
      int cur_avail_executors = get_avail_executor_num(executor_status_map);
      while (cur_avail_executors > 0 && !delay_call_queue.empty()) {
        auto delay_call = delay_call_queue.front();
        schedule_func_call(log, helper, executor_status_map, function_executor_map, delay_call.session_id_, 
                            delay_call.app_name_, delay_call.func_name_, delay_call.func_args_, delay_call.arg_flag_);
        cur_avail_executors--;
        delay_call_queue.pop();
      }
      auto cur_stamp = std::chrono::system_clock::now();

      while (!delay_call_queue.empty()){
        auto delay_call = delay_call_queue.front();
        auto delay_time = std::chrono::duration_cast<std::chrono::microseconds>(cur_stamp - delay_call.trigger_time_).count();
        if (delay_time > schedDelayTime) {
          if (!rejectExtraReq) {
            vector<string> func_name_vec;
            func_name_vec.push_back(delay_call.func_name_);
            vector<vector<string>> func_args_vec;
            func_args_vec.push_back(delay_call.func_args_);
            forward_call_via_helper(helper, delay_call.session_id_, delay_call.app_name_, 
                                func_name_vec, func_args_vec, delay_call.arg_flag_);
          }
          delay_call_queue.pop();
        }
        else{
          break;
        }
      }
    }

    report_end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(report_end - report_start).count();

    if (duration >= schedTimerThreshold) {
      // rerun check
      while (!rerun_check_queue.empty() && rerun_check_queue.top().check_time_ <= report_end) {
        auto item = rerun_check_queue.top();
        auto func_trigger_meta = item.trigger_ptr_->action_for_rerun(item.session_);
        for (auto &func_metadata : func_trigger_meta) {
          schedule_func_call(log, helper, executor_status_map, function_executor_map, item.session_, 
                            func_app_map[func_metadata.func_name_], func_metadata.func_name_, 
                            func_metadata.func_args_, func_metadata.arg_flag_);
        }
        rerun_check_queue.pop();
      }

      // TODO update function locations
      helper->update_status(get_avail_executor_num(executor_status_map), function_cache);

      report_start = std::chrono::system_clock::now();
    }
  }
  std::cout << __func__ << ": quit...\n";
}

int main(int argc, char *argv[]) {
    auto exit = [](int) {
        is_quit__.store(true, std::memory_order_release);
        shared_chan.disconnect();
        for(auto &chan_pair : executor_chans_map) {
            chan_pair.second->disconnect();
        }
        std::cout << "Exit with env cleared\n" << std::flush;
    };
    ::signal(SIGINT  , exit);
    ::signal(SIGABRT , exit);
    ::signal(SIGSEGV , exit);
    ::signal(SIGTERM , exit);
    ::signal(SIGHUP  , exit);

    // read the YAML conf
    YAML::Node conf = YAML::LoadFile("conf/config.yml");
    std::cout << "Read file config.yml" << std::endl;

    unsigned coordThreadCount = conf["threads"]["coord"].as<unsigned>();
    unsigned IOThreadCount = conf["threads"]["io"].as<unsigned>();

    funcDir = conf["func_dir"].as<string>();
    schedDelayTime = conf["delay"].as<unsigned>();
    sharedExecutor = conf["shared"].as<unsigned>() == 1;
    rejectExtraReq = conf["forward_or_reject"].as<unsigned>() == 1;

    YAML::Node user = conf["user"];
    Address ip = user["ip"].as<Address>();

    unsigned executor_num = user["executor"].as<unsigned>();

    vector<Address> coord_ips;
    YAML::Node coord = user["coord"];
    for (const YAML::Node &node : coord) {
      coord_ips.push_back(node.as<Address>());
    }

    if (coord_ips.size() <= 0) {
      std::cerr << "No coordinator found" << std::endl;
      exit(1);
    }

    vector<HandlerThread> threads;
    for (Address addr : coord_ips) {
      for (unsigned i = 0; i < coordThreadCount; i++) {
        threads.push_back(HandlerThread(addr, i));
      }
    }

    vector<Address> kvs_routing_ips;
    if (YAML::Node elb = user["routing-elb"]) {
      kvs_routing_ips.push_back(elb.as<Address>());
    }

    vector<UserRoutingThread> kvs_routing_threads;
    for (Address addr : kvs_routing_ips) {
      for (unsigned i = 0; i < 4; i++) {
        kvs_routing_threads.push_back(UserRoutingThread(addr, i));
      }
    }
    
    kvs_clients.push_back(new KvsClient(kvs_routing_threads, ip, 0, 30000));
    CommHelper helper(threads, ip, copy_to_shm_obj, get_shm_obj, IOThreadCount, 30000);

    // additional IO threads
    vector<std::thread> io_threads;

    for (unsigned thread_id = 1; thread_id < IOThreadCount; thread_id++) {
      ThreadMsgQueue msg_queue;
      secondary_thread_msg_queues.push_back(msg_queue);
      CommRespQueue resp_queue;
      resp_queues.push_back(resp_queue);
      kvs_clients.push_back(new KvsClient(kvs_routing_threads, ip, thread_id, 30000));

      io_threads.push_back(std::thread(&CommHelper::run_io_thread_loop, &helper, ip, thread_id));
    }

    std::cout << "Done Configuration" << std::endl;
    run(&helper, ip, 0, executor_num);
}