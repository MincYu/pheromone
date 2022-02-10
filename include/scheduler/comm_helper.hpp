#ifndef INCLUDE_KVS_HELPER_HPP_
#define INCLUDE_KVS_HELPER_HPP_

#include "common.hpp"
#include "anna_client/kvs_client.hpp"
#include "requests.hpp"
#include "kvs_threads.hpp"
#include "types.hpp"
#include "trigger.hpp"
#include "operation.pb.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include "safe_ptr.hpp"


enum RecvMsgType {Null, Call, DataResp, KvsGetResp, KvsPutResp};
struct RecvMsg {
  RecvMsgType msg_type_;
  string app_name_;
  string resp_address_;
  vector<string> func_name_;
  vector<vector<string>> func_args_;
  vector<int> is_func_arg_keys_;
  // pair<string, bool> session_id_sync_data_;
  string session_id_;
  bool sync_data_status_;
  string data_key_;
  unsigned data_size_;
};

struct AppInfo {
  set<string> functions_;
  map<string, set<string>> direct_deps_;
  vector<Bucket> buckets_;
};


struct InflightFuncArgs {
  string func_name_;
  string session_id_;
  vector<string> key_args_;
  set<string> inflight_args_;
  int arg_flag_;
};

struct RerunTriggerTimeout {
  TriggerPointer trigger_ptr_;
  unsigned timeout_;
};


enum SecondaryThreadMsgType {SendReq, GetKvs, PutKvs};
struct SecondaryThreadMsg {
  SecondaryThreadMsgType msg_type_;
  string key_;
  char* data_ptr_;
  unsigned data_size_;
};

using ThreadMsgQueue = sf::safe_ptr<queue<SecondaryThreadMsg>>; 
vector<ThreadMsgQueue> secondary_thread_msg_queues;

using CommRespQueue = sf::safe_ptr<queue<RecvMsg>>;
vector<CommRespQueue> resp_queues;

vector<KvsClient*> kvs_clients;
int msg_process_batch = 10;

ZmqUtil zmq_util;
ZmqUtilInterface *kZmqUtil = &zmq_util;
zmq::context_t context(8);

std::hash<string> hasher;

typedef void (* CopyFunction)(string&, const char*, unsigned);
typedef pair<char*, unsigned> (* GetFunction)(string&);


inline bool send_no_block_msg(zmq::socket_t* socket, zmq::message_t& message){
  try{
    socket->send(message, ZMQ_DONTWAIT);
    return true;
  }
  catch(std::exception& e){
    // std::cout << "Queue this message when error occurs " << e.what() << std::endl;
    return false;
  }
}

inline bool send_no_block_msg(zmq::socket_t* socket, const string& s){
  zmq::message_t msg(s.size());
  memcpy(msg.data(), s.c_str(), s.size());
  return send_no_block_msg(socket, msg);
}

class CommHelperInterface {
 public:
  // virtual void get_async(const BucketKey& bucket_key) = 0;
  virtual void notify_put(const BucketKey& bucket_key, vector<string> &active_triggers, const string& resp_address, const string& payload=emptyString) = 0;
  
  virtual void set_logger(logger log) = 0;
  virtual void update_status(int avail_executors, set<string> &function_cache) = 0;
  virtual vector<RecvMsg> try_recv_msg(map<Bucket, vector<TriggerPointer>> &bucket_triggers_map, map<string, AppInfo> &app_info_map, 
                              map<string, string> &func_app_map, map<string, string> &bucket_app_map, map<string, vector<RerunTriggerTimeout>> &func_trigger_timeout_map) = 0;
  virtual void forward_func_call(string &resp_address, string &app_name, vector<string> &func_name_vec, vector<vector<string>> &func_args_vec, int arg_flag, string &session_id) = 0;
  virtual void send_data_req(string &key) = 0;
  // virtual void send_data_resp(string &key, char* data_ptr, unsigned data_size) = 0;

  virtual void get_kvs_async(string &key) = 0;
  virtual void put_kvs_async(string &key, char* val_ptr, unsigned val_size) = 0;
  
  virtual void client_response(string &resp_address, string &app_name, string &output_data) = 0;
};

class CommHelper : public CommHelperInterface {
 public:
  CommHelper(vector<HandlerThread> routing_threads, string ip, CopyFunction copy_func, GetFunction get_func, unsigned io_thread_count = 1, unsigned timeout = 10000):
      routing_threads_(routing_threads),
      ip_(ip),
      ut_(ip, 0),
      copy_func_(copy_func),
      get_func_(get_func),
      io_thread_count_(io_thread_count),
      io_thread_id_(0),
      socket_cache_(SocketCache(&context, ZMQ_PUSH)),
      key_query_puller_(zmq::socket_t(context, ZMQ_PULL)),
      response_puller_(zmq::socket_t(context, ZMQ_PULL)),
      key_notify_puller_(zmq::socket_t(context, ZMQ_PULL)),
      update_status_puller_(zmq::socket_t(context, ZMQ_PULL)),
      data_access_server_puller_(zmq::socket_t(context, ZMQ_PULL)),
      data_access_client_puller_(zmq::socket_t(context, ZMQ_PULL)),
      func_exec_puller_(zmq::socket_t(context, ZMQ_PULL)),
      timeout_(timeout) {
    seed_ = time(NULL);
    seed_ += hasher(ip);

    key_query_puller_.bind(ut_.key_query_bind_address());
    response_puller_.bind(ut_.remote_response_bind_address());
    key_notify_puller_.bind(ut_.notify_put_bind_address());
    
    update_status_puller_.bind(ut_.trigger_update_bind_address());
    data_access_server_puller_.bind(ut_.data_access_server_bind_address());
    data_access_client_puller_.bind(ut_.data_access_client_bind_address());
    func_exec_puller_.bind(kBindBase + std::to_string(funcExecPort));
    std::cout << "Worker puller binded" << std::endl;

    pollitems_ = {
        {static_cast<void*>(key_query_puller_), 0, ZMQ_POLLIN, 0},
        {static_cast<void*>(response_puller_), 0, ZMQ_POLLIN, 0},
        {static_cast<void*>(key_notify_puller_), 0, ZMQ_POLLIN, 0}
    };

    zmq_pollitems_ = {
        {static_cast<void*>(update_status_puller_), 0, ZMQ_POLLIN, 0},
        {static_cast<void*>(func_exec_puller_), 0, ZMQ_POLLIN, 0},
        {static_cast<void*>(data_access_server_puller_), 0, ZMQ_POLLIN, 0},
        {static_cast<void*>(data_access_client_puller_), 0, ZMQ_POLLIN, 0}
    };

    // set the request ID to 0
    rid_ = 0;
  }

  ~CommHelper() { 
  }

 public:
  /**
   * Primary and secondary loop
   * 
   */
  void run_io_thread_loop(string ip, unsigned tid){
    CommHelperThread ut(ip, tid);
    int queue_idx = tid - 1;

    // zmq::context_t io_thread_context(1);

    SocketCache pushers(&context, ZMQ_PUSH);
    zmq::socket_t secondary_func_exec_puller(context, ZMQ_PULL);
    zmq::socket_t secondary_data_access_server_puller(context, ZMQ_PULL);
    zmq::socket_t secondary_data_access_client_puller(context, ZMQ_PULL);
    
    secondary_func_exec_puller.bind(kBindBase + std::to_string(funcExecPort + tid));
    secondary_data_access_server_puller.bind(ut.data_access_server_bind_address());
    secondary_data_access_client_puller.bind(ut.data_access_client_bind_address());

    // secondary_data_access_server_puller.setsockopt(ZMQ_RCVBUF, 16777216 * 3);
    // secondary_data_access_client_puller.setsockopt(ZMQ_RCVBUF, 16777216 * 3);

    vector<zmq::pollitem_t> pollitems = {
        {static_cast<void*>(secondary_func_exec_puller), 0, ZMQ_POLLIN, 0},
        {static_cast<void*>(secondary_data_access_server_puller), 0, ZMQ_POLLIN, 0},
        {static_cast<void*>(secondary_data_access_client_puller), 0, ZMQ_POLLIN, 0}
    };

    while (true) {
      kZmqUtil->poll(0, &pollitems);

      if (pollitems[0].revents & ZMQ_POLLIN){
        string serialized = kZmqUtil->recv_string(&secondary_func_exec_puller);
        RecvMsg resp;
        handle_function_call(serialized, resp, tid);
        resp_queues[queue_idx]->push(resp);
      }

      // handle data request
      if (pollitems[1].revents & ZMQ_POLLIN) {
        zmq::message_t message;
        secondary_data_access_server_puller.recv(&message);
        handle_data_request(message, pushers, tid);
      }

      // handle data response
      if (pollitems[2].revents & ZMQ_POLLIN) {
        zmq::message_t message;
        secondary_data_access_client_puller.recv(&message);
        RecvMsg resp;
        handle_data_response(message, resp, tid);
        resp_queues[queue_idx]->push(resp);
      }

      int msg_processed = 0;
      while (!secondary_thread_msg_queues[queue_idx]->empty()) {
        if (msg_processed >= msg_process_batch) break;
        auto pop_msg_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        // auto thread_msg = secondary_thread_msg_queues[queue_idx]->pop_front();
        auto thread_msg = secondary_thread_msg_queues[queue_idx]->front();
        if (thread_msg.msg_type_ == SecondaryThreadMsgType::SendReq) {
          send_data_req_base(thread_msg.key_, pushers, tid);
        }
        else if (thread_msg.msg_type_ == SecondaryThreadMsgType::GetKvs) {
          get_kvs_async_base(thread_msg.key_, tid);
        }
        else if (thread_msg.msg_type_ == SecondaryThreadMsgType::PutKvs) {
          put_kvs_async_base(thread_msg.key_, thread_msg.data_ptr_, thread_msg.data_size_, tid);
        }
        secondary_thread_msg_queues[queue_idx]->pop();
        auto done_msg_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        msg_processed++;
        log_->info("Msg processed io_thread {}. pop: {}, done: {}.", tid, pop_msg_stamp, done_msg_stamp);
      }

      vector<RecvMsg> comm_resps;
      get_kvs_responses(comm_resps, tid);
      for (auto &comm_resp : comm_resps){
        resp_queues[queue_idx]->push(comm_resp);
      }
    }
  }

  vector<RecvMsg> try_recv_msg(map<Bucket, vector<TriggerPointer>> &bucket_triggers_map, map<string, AppInfo> &app_info_map, 
                      map<string, string> &func_app_map, map<string, string> &bucket_app_map,  map<string, vector<RerunTriggerTimeout>> &func_trigger_timeout_map){
    kZmqUtil->poll(0, &zmq_pollitems_);

    vector<RecvMsg> comm_resps;
    if (zmq_pollitems_[0].revents & ZMQ_POLLIN) {
      string serialized = kZmqUtil->recv_string(&update_status_puller_);

      UpdateCoordMsg coord_msg;
      coord_msg.ParseFromString(serialized);
      int update_msg_type = coord_msg.msg_type();
      if (update_msg_type == 0){
        // add dependency and triggers
        string coord_ip = coord_msg.ip();
        unsigned thread_id = coord_msg.thread_id();
        HandlerThread coord_thread(coord_ip, thread_id);

        string app_name = coord_msg.app_name();
        // TODO avoid creating too many same HanderThread
        app_coord_map_[app_name] = coord_thread;
        for (auto &func : coord_msg.functions()){
          app_info_map[app_name].functions_.insert(func);
          func_app_map[func] = app_name;
        }

        for (auto &dep : coord_msg.dependencies()){
          if (dep.type() != DependencyType::DIRECT) {
            log_->warn("Non-Direct dependency {} detected in coord_msg for app: {}.", dep.type(), app_name);
            continue;
          }
          auto src_func = dep.src_functions(0);
          for (int i = 0; i < dep.tgt_functions_size(); i++){
            app_info_map[app_name].direct_deps_[src_func].insert(dep.tgt_functions(i));
          }
        }

        for (const auto &trigger : coord_msg.triggers()) {
          Bucket bucket_name = trigger.bucket_name();
          bucket_coord_map_[bucket_name] = coord_thread;
          app_info_map[app_name].buckets_.push_back(bucket_name);
          bucket_app_map[bucket_name] = app_name;
          string trigger_name = trigger.trigger_name();
          
          if (!trigger_name.empty()){
            auto trigger_ptr = gen_trigger_pointer(trigger.primitive_type(), trigger_name, trigger.primitive());
            if (trigger_ptr != nullptr){
              trigger_ptr->set_trigger_option(trigger.trigger_option());
              bucket_triggers_map[bucket_name].push_back(trigger_ptr);
              vector<RerunMetadata> rerun_meta;
              for (const auto &hint : trigger.hints()) {
                rerun_meta.push_back({hint.source_function(), hint.source_key(), hint.timeout()});
                // TODO avoid rerun multuple times while still performing at the view of triggers
                func_trigger_timeout_map[hint.source_function()].push_back({trigger_ptr, hint.timeout()});
              }
              trigger_ptr->set_rerun_metadata(rerun_meta);
            }
          }
        }
      }
      else if (update_msg_type == 1) {
        // sync trigger msg for clear
        for (const auto &trigger_msg : coord_msg.triggers()) {
          Bucket bucket_name = trigger_msg.bucket_name();
          string trigger_name = trigger_msg.trigger_name();
          for (auto &trigger : bucket_triggers_map[bucket_name]) {
            if (trigger->get_trigger_name() == trigger_name) trigger->clear(trigger_msg.session());
          }
        }
      }
      else if (update_msg_type == 2) {
        // delete triggers
        for (const auto &trigger : coord_msg.triggers()) {
          Bucket bucket_name = trigger.bucket_name();
          string trigger_name = trigger.trigger_name();
          
          auto iter = bucket_triggers_map[bucket_name].begin();
          while((*iter)->get_trigger_name() != trigger_name) iter++;
          if ( iter != bucket_triggers_map[bucket_name].end()) {
            bucket_triggers_map[bucket_name].erase(iter);
          }
          // TODO delete func_trigger_timeout_map
        }
      }

    }

    // function call
    if (zmq_pollitems_[1].revents & ZMQ_POLLIN) {
      string serialized = kZmqUtil->recv_string(&func_exec_puller_);
      RecvMsg resp;
      handle_function_call(serialized, resp);
      comm_resps.push_back(resp);
    }

    // data access server to handle data request
    if (zmq_pollitems_[2].revents & ZMQ_POLLIN) {
      zmq::message_t message;
      data_access_server_puller_.recv(&message);
      handle_data_request(message, socket_cache_);
    }
  
    // data access client to handle data response
    if (zmq_pollitems_[3].revents & ZMQ_POLLIN) {
      zmq::message_t message;
      data_access_client_puller_.recv(&message);
      RecvMsg resp;
      handle_data_response(message, resp);
      comm_resps.push_back(resp);
    }

    get_kvs_responses(comm_resps);

    for (auto &resp_queue: resp_queues){
      while (!resp_queue->empty()) {
        // comm_resps.push_back(resp_queue->pop_front());
        comm_resps.push_back(resp_queue->front());
        resp_queue->pop();
      }
    }

    return comm_resps;
  }

  /**
   * Other interfaces
   *
   */ 
  void client_response(string &resp_address, string &app_name, string &output_data) {
    FunctionCallResponse client_resp;
    client_resp.set_app_name(app_name);
    client_resp.set_error_no(0);
    client_resp.set_output(output_data);

    send_request(client_resp, socket_cache_[resp_address]);
  }

  void update_status(int avail_executors, set<string> &function_cache) {
    UpdateStatusMessage msg;
    msg.set_ip(ip_);
    msg.set_avail_executors(avail_executors);
    for (auto &function : function_cache){
      msg.add_functions(function);
    }

    string serialized;
    msg.SerializeToString(&serialized);

    // update status to every coordinator
    // TODO key query and notif should be modified accordingly
    for (auto &ht : routing_threads_){
      // send_no_block_msg(&socket_cache_[ht.update_handler_connect_address()], serialized);
      kZmqUtil->send_string(serialized, &socket_cache_[ht.update_handler_connect_address()]);
    }
  }

  void forward_func_call(string &resp_address, string &app_name, vector<string> &func_name_vec, vector<vector<string>> &func_args_vec, int arg_flag, string &session_id) {
    FunctionCall forwardCall;
    forwardCall.set_app_name(app_name);
    forwardCall.set_resp_address(resp_address);
    forwardCall.set_source(ip_);
    forwardCall.set_session_id(session_id);
    forwardCall.set_sync_data_status(true);

    for (int func_i = 0; func_i < func_name_vec.size(); func_i++){
      auto req = forwardCall.add_requests();
      req->set_name(func_name_vec[func_i]);
      // selective data packing (key/original data)
      auto &func_args = func_args_vec[func_i];
      if (arg_flag > 0){
        for (int i = 0; i < func_args.size(); i+=3){
          auto arg = req->add_arguments();
          string key_name = func_args[i] + kDelimiter + func_args[i + 1];
          arg->set_body(key_name);
          arg->set_arg_flag(arg_flag);
          arg->set_data_address(get_data_server_addr(key_name));
        }
      }
      else {
        for (auto &func_arg: func_args){
          auto arg = req->add_arguments();
          arg->set_body(func_arg);
          arg->set_arg_flag(arg_flag);
        }
      }
    }

    send_request(forwardCall, socket_cache_[app_coord_map_[app_name].forward_func_connect_address()]);
  }

  void get_kvs_async(string &key){
    if (io_thread_count_ > 1){
      unsigned thread_id = get_io_thread_id(true);
      if (thread_id == 0){
        get_kvs_async_base(key);
      }
      else {
        SecondaryThreadMsg msg = {SecondaryThreadMsgType::GetKvs, key, NULL, 0};
        secondary_thread_msg_queues[thread_id - 1]->push(msg);
      }
    }
    else{
      get_kvs_async_base(key);
    }
  }

  void put_kvs_async(string &key, char* val_ptr, unsigned val_size){
    if (io_thread_count_ > 1){
      unsigned thread_id = get_io_thread_id(true);
      if (thread_id == 0){
        put_kvs_async_base(key, val_ptr, val_size);
      }
      else {
        SecondaryThreadMsg msg = {SecondaryThreadMsgType::PutKvs, key, val_ptr, val_size};
        secondary_thread_msg_queues[thread_id - 1]->push(msg);
      }
    }
    else{
      put_kvs_async_base(key, val_ptr, val_size);
    }
  }

  void send_data_req(string &key){
    if (io_thread_count_ > 1){
      // select io thread in a round-robin manner
      unsigned thread_id = get_io_thread_id(true);
      // main io thread
      if (thread_id == 0){
        send_data_req_base(key, socket_cache_);
      }
      else {
        log_->info("Push send request msg to thread {}", thread_id);
        SecondaryThreadMsg msg = {SecondaryThreadMsgType::SendReq, key, NULL, 0};
        secondary_thread_msg_queues[thread_id - 1]->push(msg);
      }
    }
    else{
      send_data_req_base(key, socket_cache_);
    }
  }

  void get_kvs_responses(vector<RecvMsg> &comm_resps, unsigned tid=0){
    vector<KeyResponse> responses = kvs_clients[tid]->receive_async();
    for (auto &response : responses){
      Key key = response.tuples(0).key();

      if (response.error() == AnnaError::TIMEOUT) {
        log_->info("Kvs request io_thread {} for key {} timed out.", tid, key);
        if (response.type() == RequestType::GET) {
          kvs_clients[tid]->get_async(key);
        } 
        else {
          // TODO re-issue put request
        }
      } 
      else {
        log_->info("Thread {} Kvs response type {} error code {}", tid, response.type(), response.tuples(0).error());
        if (response.type() == RequestType::GET && response.tuples(0).error() != 1) {
          auto kvs_recv_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::system_clock::now().time_since_epoch()).count();
          RecvMsg resp;
          resp.msg_type_ = RecvMsgType::KvsGetResp;

          LWWPairLattice<string> lww_lattice = deserialize_lww(response.tuples(0).payload());

          resp.data_key_ = key;
          resp.data_size_ = lww_lattice.reveal().value.size();
          string local_key_name = kvsKeyPrefix + kDelimiter + key;
          
          copy_func_(local_key_name, lww_lattice.reveal().value.c_str(), resp.data_size_);
          comm_resps.push_back(resp);

          auto kvs_get_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::system_clock::now().time_since_epoch()).count();
          log_->info("Kvs GET response io_thread {}. recv: {}, copy: {}.", tid, kvs_recv_stamp, kvs_get_stamp);
        } 
        else if (response.type() == RequestType::PUT){
          auto kvs_recv_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::system_clock::now().time_since_epoch()).count();
          RecvMsg resp;
          resp.msg_type_ = RecvMsgType::KvsPutResp;
          resp.data_key_ = key;
          comm_resps.push_back(resp);
          log_->info("Kvs PUT response io_thread {}. recv: {}.", tid, kvs_recv_stamp);
        }
      }
    }
  }

  /**
   * Issue an async PUT notifying request to the coordinator.
   */
  void notify_put(const BucketKey& bucket_key, vector<string> &active_triggers, const string& resp_address, const string& payload=emptyString) {
    notify_put_base(bucket_key, active_triggers, resp_address, socket_cache_, payload, 0);
  }

  /**
   * Set the logger used by the client.
   */
  void set_logger(logger log) { log_ = log; }

  /**
   * Clears the key address cache held by this client.
   */
  void clear_cache() { key_address_cache_->clear(); }

  /**
   * Return the random seed used by this client.
   */
  unsigned get_seed() { return seed_; }

 private:

  /**
   * Based handling methods
   */
  void handle_function_call(string& serialized, RecvMsg& resp, unsigned tid = 0){
    auto receive_call_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();

    FunctionCall call;
    call.ParseFromString(serialized);
    resp.app_name_ = call.app_name();
    resp.msg_type_ = RecvMsgType::Call;
    resp.resp_address_ = call.resp_address();
    resp.session_id_ = call.session_id();
    resp.sync_data_status_ = call.sync_data_status();

    for (auto &req : call.requests()){
      resp.func_name_.push_back(req.name());
      vector<string> args;
      int arg_flag = 0;
      for (auto &arg : req.arguments()){
        args.push_back(arg.body());
        if (arg.arg_flag() > 0) {
          arg_flag = arg.arg_flag();
          key_address_cache_->emplace(arg.body(), arg.data_address());
        }
      }
      resp.is_func_arg_keys_.push_back(arg_flag);
      resp.func_args_.push_back(args);
    }
    string name_to_log = resp.func_name_.size() == 1 ? resp.func_name_[0] : resp.app_name_;
    log_->info("Function call {} io_thread {}. num: {}, recv: {}.", name_to_log, tid, resp.func_name_.size(), receive_call_stamp);
  }

  void handle_data_request(zmq::message_t& message, SocketCache& pushers, unsigned tid = 0){
    auto data_req_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    auto req_str = static_cast<char*>(message.data());

    auto key_len = static_cast<uint8_t>(req_str[0]);
    string key(req_str + 1, key_len);
    string resp_address(req_str + key_len + 1, message.size() - key_len - 1);

    auto parse_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // response
    string msg_head;
    msg_head.push_back(key_len);
    msg_head += key;

    auto data_info_pair = get_func_(key);

    auto get_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (data_info_pair.second > 0){
      // we use low-level interface to avoid data copy
      auto msg_len = key_len + data_info_pair.second + 1;
      zmq::message_t msg(msg_len);
      memcpy(msg.data(), msg_head.c_str(), key_len + 1);
      memcpy(static_cast<char*>(msg.data()) + key_len + 1, data_info_pair.first, data_info_pair.second);

      auto send_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
      pushers[resp_address].send(msg);

      auto finish_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();

      log_->info("Data request io_thread {}. key: {}, size: {}, recv: {}, parse: {}, get: {}, send: {}, finish: {}", 
                tid, key, data_info_pair.second, data_req_stamp, parse_stamp, get_stamp, send_stamp, finish_stamp);
    }
    else{
      log_->info("Data request io_thread {} w/o local data found. key: {}, recv: {}.", tid, key, data_req_stamp);
    }
  }

  void handle_data_response(zmq::message_t& message, RecvMsg& resp, unsigned tid = 0){
    auto data_resp_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto resp_str = static_cast<char*>(message.data());
    resp.msg_type_ = RecvMsgType::DataResp;
    auto key_len = static_cast<uint8_t>(resp_str[0]);
    string key(resp_str + 1, key_len);

    auto parse_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    resp.data_key_ = key;
    resp.data_size_ = message.size() - key_len - 1;
    copy_func_(key, resp_str + key_len + 1, resp.data_size_);

    auto data_copy_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    log_->info("Data response io_thread {}. key: {}, size: {}, recv: {}, parse: {}, copy: {}.", 
              tid, key, resp.data_size_, data_resp_stamp, parse_stamp, data_copy_stamp);
  }

  void send_data_req_base(string &key, SocketCache &pushers, unsigned tid = 0){
    auto start_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
    string data_address = get_key_address(key);
    if (data_address.length() == 0) {
      // TODO
      // if (pending_key_query_map_.find(key_string) == pending_key_query_map_.end()) {
      //   pending_key_query_map_[key_string].first = std::chrono::system_clock::now();
      // }
      // pending_key_query_map_[key_string].second = request;
      log_->info("No data location for key {}", key);
      return;
    }
    auto key_len = static_cast<uint8_t>(key.size());
    string data_request;
    data_request.push_back(key_len);
    data_request += key;
    data_request += "tcp://" + ip_ + ":" + std::to_string(tid + dataAccessClientPort);

    // kZmqUtil->send_string(data_request, &pushers[data_address]);
    auto msg = kZmqUtil->string_to_message(data_request);
    auto parse_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();

    pushers[data_address].send(msg);
  
    auto send_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
    log_->info("Thread {} send data request for key {} start {} parse {} at {}.", tid, key, start_stamp, parse_stamp, send_stamp);
  }

  void get_kvs_async_base(string &key, unsigned tid = 0) {
    kvs_clients[tid]->get_async(key);
    log_->info("Thread {} issue async GET key {}", tid, key);
  }

  void put_kvs_async_base(string &key, char* val_ptr, unsigned val_size, unsigned tid = 0){
    string rid = kvs_clients[tid]->put_async(key, serialize(generate_timestamp(tid), string{val_ptr, val_size}), LatticeType::LWW);
    log_->info("Thread {} issue async PUT key {}", tid, key);
  }

  void notify_put_base(const BucketKey& bucket_key, vector<string> &active_triggers, const string& resp_address, SocketCache &pushers, const string& payload=emptyString, unsigned tid = 0) {
    Address notif_thread = get_notifying_thread(bucket_key.bucket_);

    if (notif_thread.empty()) {
      log_->info("No coordinator thread found when sync on bucket {}", bucket_key.bucket_);
      return;
    }

    KeyNotifyRequest key_notif_requst;

    string req_id = get_request_id();
    key_notif_requst.set_request_id(req_id);
    key_notif_requst.set_response_address(resp_address);
    // key_notif_requst.set_response_address(ut_.notify_put_connect_address());

    for (auto &active_trigger : active_triggers){
      key_notif_requst.add_active_trigger_names(active_trigger);
    }

    BucketKeyAddress *addr = key_notif_requst.add_addresses();
    addr = get_addr_from_bucket_key(bucket_key, addr);
    addr->add_ips(get_data_server_addr(bucket_key.shm_key_name(), true));

    if (!payload.empty()){
      addr->set_payload(payload);
    }

    string serialized;
    key_notif_requst.SerializeToString(&serialized);

    kZmqUtil->send_string(serialized, &pushers[notif_thread]);
  }


  /**
   * other assistant methods
   */
  string get_key_address(const string &key) {
    if (key_address_cache_->find(key) == key_address_cache_->end()) {
      return "";
    }
    else {
      // return *(next(begin(key_address_cache_[key]), rand_r(&seed_) % key_address_cache_[key].size()));
      return key_address_cache_->at(key);
    }
  }

  string get_data_server_addr(string key_name, bool exclude_main_thread = false){
    // int thread_id = 0;
    // if (io_thread_count_ > 1){
    //   thread_id = exclude_main_thread ? hasher(key_name) % (io_thread_count_ - 1) + 1 : hasher(key_name) % io_thread_count_;
    // }
    auto thread_id = get_io_thread_id(exclude_main_thread);
    return "tcp://" + ip_ + ":" + std::to_string(thread_id + dataAccessServerPort);
  }

  unsigned get_io_thread_id(bool exclude_main_thread = false) {
    if (++io_thread_id_ % io_thread_count_ == 0) {
      io_thread_id_ = exclude_main_thread ? 1 : 0;
    }
    return io_thread_id_;
  }

  /**
   * Send a query to the routing tier asynchronously.
   */
  void key_query_async(const BucketKey& bucket_key) {
    KeyQueryRequest request;

    request.set_request_id(get_request_id());
    request.set_response_address(ut_.key_query_connect_address());
    BucketKeyAddress *addr = request.add_addresses();
    addr = get_addr_from_bucket_key(bucket_key, addr);

    Address qry_thread = get_querying_thread(bucket_key.bucket_);
    send_request<KeyQueryRequest>(request, socket_cache_[qry_thread]);
  }

  /**
   * Returns one random routing thread's key address connection address. 
   * Currently we only have a single coordinator.
   */
  Address get_querying_thread(Bucket bucket) {
    if (bucket_coord_map_.find(bucket) == bucket_coord_map_.end()) {
      return "";
    }
    else {
      return bucket_coord_map_[bucket].key_query_handler_connect_address();
    }
  }

  Address get_notifying_thread(Bucket bucket) {
    if (bucket_coord_map_.find(bucket) == bucket_coord_map_.end()) {
      return "";
    }
    else {
      return bucket_coord_map_[bucket].notify_handler_connect_address();
    }
  }

  /**
   * Generates a unique request ID.
   */
  string get_request_id() {
    if (++rid_ % 10000 == 0) rid_ = 0;
    return ut_.ip() + ":" + std::to_string(ut_.tid()) + "_" +
           std::to_string(rid_++);
  }

 public:
   // cache for retrieved worker addresses organized by key
  sf::safe_ptr<map<string, Address>> key_address_cache_;
  sf::safe_ptr<map<string, set<Address>>> key_requestor_map_;
  
  // class logger
  logger log_;

 private:
  // the set of routing addresses outside the cluster
  vector<HandlerThread> routing_threads_;

  map<string, HandlerThread> app_coord_map_;
  map<string, HandlerThread> bucket_coord_map_;

  // the current request id
  unsigned rid_;

  unsigned io_thread_count_;
  unsigned io_thread_id_;

  // the random seed for this client
  unsigned seed_;

  // the IP and port functions for this thread
  CommHelperThread ut_;

  // cache for opened sockets
  SocketCache socket_cache_;

  // ZMQ receivikey_notify_puller_ng sockets
  zmq::socket_t key_query_puller_;
  zmq::socket_t response_puller_;
  zmq::socket_t key_notify_puller_;
  zmq::socket_t update_status_puller_;
  zmq::socket_t data_access_server_puller_;
  zmq::socket_t data_access_client_puller_;
  zmq::socket_t func_exec_puller_;

  vector<zmq::pollitem_t> pollitems_;
  vector<zmq::pollitem_t> zmq_pollitems_;

  CopyFunction copy_func_;
  GetFunction get_func_;

  string ip_;

  // GC timeout
  unsigned timeout_;
};



#endif  // INCLUDE_KVS_HELPER_HPP_

