#ifndef INCLUDE_EXECUTOR_FUNCTION_LIB_HPP_
#define INCLUDE_EXECUTOR_FUNCTION_LIB_HPP_

#include "common.hpp"
#include "requests.hpp"
#include "kvs_threads.hpp"
#include "types.hpp"
#include "trigger.hpp"
#include "operation.pb.h"
#include "cpp_function.hpp"
#include <memory>

using shm_chan_t = ipc::chan<ipc::relat::multi, ipc::relat::multi, ipc::trans::unicast>;

constexpr char const kvs_name__  [] = "ipc-kvs";
shm_chan_t shared_chan { kvs_name__, ipc::sender };

shm_chan_t* local_chan;

typedef int (*CppFunction)(UserLibraryInterface*, int, char**);

class EpheObjectImpl : public EpheObject {
  public:
    EpheObjectImpl(string obj_name, size_t size, bool create) {
      size_ = size;
      obj_name_ = obj_name;
      if (create) {
        // shm_id_ = ipc::shm::acquire(obj_name_.c_str(), size_, ipc::shm::create);
        shm_id_ = ipc::shm::acquire(obj_name_.c_str(), size_);
      }
      else{
        shm_id_ = ipc::shm::acquire(obj_name_.c_str(), size_, ipc::shm::open);
      }
      value_ = ipc::shm::get_mem(shm_id_, nullptr);
      target_func_ = "";
    }

    EpheObjectImpl(string bucket, string key, string session_id, size_t size, bool create)
     :EpheObjectImpl(get_local_object_name(bucket, key, session_id), size, create) {}

    EpheObjectImpl(string src_function, string tgt_function, string session_id, size_t size)
     :EpheObjectImpl("b_" +  tgt_function, "k_" + src_function, session_id, size, true) {
      target_func_ = tgt_function;
    }

    EpheObjectImpl(const EpheObjectImpl& other){
      obj_name_ = other.obj_name_;
      size_ = other.size_;
      value_ = other.value_;
      target_func_ = other.target_func_;
    }

    ~EpheObjectImpl(){
      ipc::shm::release(shm_id_);
    }

    void* get_value(){
      return value_;
    }

    void set_value(const void* value, size_t val_size){
      memcpy(value_, value, val_size);
    }

    void update_size(size_t size){
      size_ = size;
    }

    size_t get_size(){
      return size_;
    }

  public:
    string obj_name_;
    size_t size_;
    string target_func_;

  private:
    void* value_;
    ipc::shm::id_t shm_id_;
};

class UserLibrary : public UserLibraryInterface {
  public:
    UserLibrary(string ip, unsigned thread_id) {
      ip_number_ = ip;
      chan_id_ = static_cast<uint8_t>(thread_id + 1);
      rid_ = 0;
      object_id_ = 0;
    }

    ~UserLibrary() {}

  public:
    void set_function_name(string &function){
      function_ = function;
    }

    void set_session_id(string &session_id){
      session_id_ = session_id;
    }

    void set_persist_flag(uint8_t &persist_flag){
      persist_flag_ = persist_flag;
    }
    
    void collect_session(const string &session) {
      if (session == session_id_) collect_current_session_ = true;
      else session_objects_.erase(session);
    }

    void clear_session(){
      if (collect_current_session_) session_objects_.erase(session_id_);
      collect_current_session_ = false;
      function_ = emptyString;
      session_id_ = emptyString;
      persist_flag_ = 0;
      size_of_args_.clear();
    }

    void add_arg_size(size_t size){
      size_of_args_.push_back(size);
    }

    size_t get_size_of_arg(int arg_idx){
      if (size_of_args_.size() <= arg_idx) return -1;
      return size_of_args_[arg_idx];
    }

    EpheObject* create_object(string bucket, string key, size_t size = 1024 * 1024) {
      return own(new EpheObjectImpl(bucket, key, session_id_, size, true));
    }

    EpheObject* create_object(size_t size = 1024 * 1024) {
      return create_object(bucketNameDirectInvoc, gen_unique_key(), size);
    }

    EpheObject* create_object(string target_function, bool many_to_one_trigger = true, size_t size = 1024 * 1024) {
      if (many_to_one_trigger) {
        return own(new EpheObjectImpl(function_, target_function, session_id_, size));
      }
      else {
        return create_object("b_" + target_function, gen_unique_key(), size);
      }
    }

    void send_object(EpheObject *data, bool output = false) {
      string req;
      auto req_id = get_request_id();
      req.push_back(chan_id_);

      bool wait_res = false;
      if (output) {
        if (persist_flag_ == 1) {
          // write to remote data store
          req.push_back(3);
          wait_res = true;
        }
        else {
          // directly response to client
          req.push_back(4);
        }
      }
      else {
        // ephemeral data
        req.push_back(2);
      }
      
      req.push_back(req_id);
      bool data_packing = data->get_size() <= msgDataPackingThreshold;
      req.push_back(data_packing ? 1 : 2);
      req += session_id_ + "|";
      req += function_ + "|" + static_cast<EpheObjectImpl*>(data)->target_func_ + "|"
                        + static_cast<EpheObjectImpl*>(data)->obj_name_ + "|";

      if (data_packing) {
        req += string(static_cast<char*>(data->get_value()), data->get_size());
      }
      else {
        req += std::to_string(data->get_size());
      }
      // while (!shared_chan.send(req)) {
      //     shared_chan.wait_for_recv(1);
      // }
      
      bool send_res = shared_chan.send(req);
      if (!send_res) {
        std::cerr << "Send to executer failed.";

        if (!shared_chan.wait_for_recv(1)) {
            std::cerr << "Wait receiver failed.\n";
        }
      }

      if (send_res && wait_res) {
        auto recv_dd = receive_response();
        auto recv_str = static_cast<char*>(recv_dd.data());

        if (recv_str == nullptr) {
          std::cout << "Put error: timeout " << std::endl;
        }
        if (recv_str[0] != 3) {
          std::cout << "Put error: incorrect message type " << (uint8_t) recv_str[0] << std::endl;
        }
        if (recv_str[1] != req_id) {
          std::cout << "Put error: request id " << (uint8_t) recv_str[1] << " doesn't match " << req_id << std::endl;
        }

        if (recv_str[2] != 1) {
          std::cout << "Put error: kvs error" << std::endl;
        }
      }
    }

    EpheObject* get_object(string bucket, string key, bool from_ephe_store=true) {
      string obj_name = get_local_object_name(bucket, key, session_id_);
      if (!from_ephe_store) {
        obj_name = kvsKeyPrefix + kDelimiter + key;
      }
      string req;
      auto req_id = get_request_id();
      req.push_back(chan_id_);
      req.push_back(1);
      req.push_back(from_ephe_store ? 1 : 2);
      req.push_back(req_id);
      req.push_back(static_cast<uint8_t>(obj_name.size()));
      req += obj_name;

      while (!shared_chan.send(req)) {
          shared_chan.wait_for_recv(1);
      }

      auto recv_dd = receive_response();
      auto recv_str = static_cast<char*>(recv_dd.data());

      if (recv_str == nullptr) {
        std::cout << "Get error: timeout for " << obj_name << std::endl;
        return nullptr;
      }
      if (recv_str[0] != 3) {
        std::cout << "Get error: incorrect message type " << (uint8_t) recv_str[0] << std::endl;
        return nullptr;
      }
      if (recv_str[1] != req_id) {
        std::cout << "Get error: request id " << (uint8_t) recv_str[1] << " doesn't match " << req_id << std::endl;
        return nullptr;
      }

      if (recv_str[2] != 1) {
        std::cout << "Get error: kvs error" << std::endl;
        return nullptr;
      }

      auto size_len = stoi(string(recv_str + 3));
      return own(new EpheObjectImpl(obj_name, size_len, false));
    }

    string gen_unique_key(){
      return ip_number_ + "_" + std::to_string(chan_id_) + "_" + std::to_string(get_object_id());
    }

  private:
    // GC notifications share the response channel and may arrive during an RPC.
    ipc::buff_t receive_response() {
      auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
      while (true) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) return {};
        auto response = local_chan->recv(remaining);
        auto data = static_cast<char*>(response.data());
        if (data == nullptr || data[0] != 4) return response;
        collect_session(string(data + 1));
      }
    }

    EpheObjectImpl* own(EpheObjectImpl *object) {
      session_objects_[session_id_].emplace_back(object);
      return object;
    }

    uint8_t get_request_id() {
      if (++rid_ % 100 == 0) {
        rid_ = 0;
      }
      return rid_;
    }

    unsigned get_object_id() {
      return object_id_++;
    }

  private:
    string ip_number_;
    uint8_t chan_id_;
    uint8_t rid_;
    unsigned object_id_;
    string function_;
    string session_id_;
    uint8_t persist_flag_;
    vector<size_t> size_of_args_;
    // Keep objects available to downstream functions until workflow completion.
    map<string, vector<std::unique_ptr<EpheObjectImpl>>> session_objects_;
    bool collect_current_session_ = false;
};

#endif  // INCLUDE_EXECUTOR_FUNCTION_LIB_HPP_

