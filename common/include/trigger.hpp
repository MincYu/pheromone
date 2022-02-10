#ifndef INCLUDE_TRIGGER_HPP_
#define INCLUDE_TRIGGER_HPP_

#include "types.hpp"
#include "operation.pb.h"

struct TriggerAction {
  string function_;

  // session is an emtpy string if not applied
  vector<pair<Session, Key>> session_keys_;

  // // it indicates whether the new object will be read in future
  // // this field is to count reference number for GC use
  // bool will_read_;
};

struct TriggerFunctionMetadata {
  string func_name_;
  vector<string> func_args_;
  int arg_flag_;
};

struct RerunMetadata {
  string source_func_;
  string source_key_;
  unsigned timeout_;
};

class Trigger {
  protected:
    string target_function_;
    string trigger_name_;
    PrimitiveType type_;
    // trigger the function using the value (0), or key name and value (1), or key name only (2)
    int trigger_option_;
    
  public:
    Trigger(const string &function_name, const string &trigger_name, const PrimitiveType type) 
      : target_function_(function_name),
        trigger_name_(trigger_name),
        type_(type),
        trigger_option_(0) {}

    virtual ~Trigger() = default;

    virtual vector<TriggerAction> action_for_new_object(const BucketKey &bucket_key) = 0;
    virtual string dump_pritimive() = 0;
    virtual void clear(const string &session) = 0;

    virtual void notify_source_func(string &func_name, string &session_id, vector<string> &func_args, int arg_flag) {};
    virtual vector<TriggerFunctionMetadata> action_for_rerun(string &session_id) { return {}; };
    virtual void set_rerun_metadata(vector<RerunMetadata> &rerun_metadata) {};

    string get_trigger_name(){ return trigger_name_;}
    PrimitiveType get_type(){ return type_;}
    void set_trigger_option(int option) {trigger_option_ = option;}
    int get_trigger_option() {return trigger_option_;}
};

using TriggerPointer = std::shared_ptr<Trigger>;

class ImmediateTrigger : public Trigger {
  public:
    ImmediateTrigger(const string &function_name, const string &trigger_name): 
      Trigger(function_name, trigger_name, PrimitiveType::IMMEDIATE) {}

    vector<TriggerAction> action_for_new_object(const BucketKey &bucket_key) {
      session_status_cache_.erase(bucket_key.session_);
      TriggerAction action = {target_function_, {std::make_pair(bucket_key.session_, bucket_key.key_)}};
      return vector<TriggerAction>{action};
    }

    void notify_source_func(string &func_name, string &session_id, vector<string> &func_args, int arg_flag) {
      session_status_cache_[session_id] = {func_name, func_args, arg_flag};
    }

    vector<TriggerFunctionMetadata> action_for_rerun(string &session_id) {
      if (session_status_cache_.find(session_id) != session_status_cache_.end()){
        return {session_status_cache_[session_id]};
      }
      return {};
    }
  
    string dump_pritimive(){
      ImmediatePrimitive primitive;
      primitive.set_function(target_function_);

      string prm_serialized;
      primitive.SerializeToString(&prm_serialized);
      return prm_serialized;
    }

    void clear(const string &session) {}
  
  private:
    map<string, TriggerFunctionMetadata> session_status_cache_;
};

class ByNameTrigger : public Trigger {
  public:
    ByNameTrigger(const string &function_name, const string &trigger_name, const string &key_name): 
      Trigger(function_name, trigger_name, PrimitiveType::BY_NAME),
      key_name_(key_name) {}

    vector<TriggerAction> action_for_new_object(const BucketKey &bucket_key) {
      session_status_cache_.erase(bucket_key.session_);
      vector<TriggerAction> actions;
      if (bucket_key.key_ == key_name_) {
        TriggerAction action;
        action.function_ = target_function_;
        action.session_keys_ = {std::make_pair(bucket_key.session_, bucket_key.key_)};
        actions.push_back(action);
      }

      return actions;
    }

    void notify_source_func(string &func_name, string &session_id, vector<string> &func_args, int arg_flag) {
      session_status_cache_[session_id] = {func_name, func_args, arg_flag};
    }

    vector<TriggerFunctionMetadata> action_for_rerun(string &session_id) {
      if (session_status_cache_.find(session_id) != session_status_cache_.end()){
        return {session_status_cache_[session_id]};
      }
      return {};
    }
  
    string dump_pritimive(){
      ByNamePrimitive primitive;
      primitive.set_function(target_function_);
      primitive.set_key_name(key_name_);

      string prm_serialized;
      primitive.SerializeToString(&prm_serialized);
      return prm_serialized;
    }
  
    void clear(const string &session) {}

  private:
    string key_name_;
    map<string, TriggerFunctionMetadata> session_status_cache_;
};

class ByBatchSizeTrigger : public Trigger {
  public:
    ByBatchSizeTrigger(const string &function_name, const string &trigger_name, int count): 
      Trigger(function_name, trigger_name, PrimitiveType::BY_BATCH_SIZE),
      count_(count) {}

    vector<TriggerAction> action_for_new_object(const BucketKey &bucket_key) {
      vector<TriggerAction> actions;
      
      buffer_.push_back(std::make_pair(bucket_key.session_, bucket_key.key_));
      // clear the session status cache
      session_status_cache_.erase(bucket_key.session_);

      if (buffer_.size() >= count_) {
        TriggerAction action;
        action.function_ = target_function_;
        action.session_keys_ = std::move(buffer_);
        actions.push_back(action);
        buffer_.clear();
      }
      
      return actions;
    }

    void notify_source_func(string &func_name, string &session_id, vector<string> &func_args, int arg_flag) {
      session_status_cache_[session_id] = {func_name, func_args, arg_flag};
    }

    vector<TriggerFunctionMetadata> action_for_rerun(string &session_id) {
      if (session_status_cache_.find(session_id) != session_status_cache_.end()){
        return {session_status_cache_[session_id]};
      }
      return {};
    }
  
    string dump_pritimive(){
      ByBatchSizePrimitive primitive;
      primitive.set_function(target_function_);
      primitive.set_count(count_);

      string prm_serialized;
      primitive.SerializeToString(&prm_serialized);
      return prm_serialized;
    }

    void clear(const string &session) {
      buffer_.clear();
      session_status_cache_.clear();
    };

  private:
    int count_;
    vector<pair<Session, Key>> buffer_;
    map<string, TriggerFunctionMetadata> session_status_cache_;
};

class BySetTrigger : public Trigger {
  public:
    BySetTrigger(const string &function_name, const string &trigger_name, const set<string> &key_set): 
      Trigger(function_name, trigger_name, PrimitiveType::BY_SET),
      key_set_(key_set) {}

    vector<TriggerAction> action_for_new_object(const BucketKey &bucket_key) {
      vector<TriggerAction> actions;
      
      if (key_set_.find(bucket_key.key_) != key_set_.end()) {
        session_prepared_keys_map_[bucket_key.session_].insert(bucket_key.key_);
        session_status_cache_.erase(bucket_key.key_ + bucket_key.session_);
        if (session_prepared_keys_map_[bucket_key.session_].size() == key_set_.size()) {
          TriggerAction action;
          action.function_ = target_function_;
          for (auto &key: key_set_){
            action.session_keys_.push_back(std::make_pair(bucket_key.session_, key));
          }
          actions.push_back(action);
          session_prepared_keys_map_.erase(bucket_key.session_);
        }
      }
      return actions;
    }

    void notify_source_func(string &func_name, string &session_id, vector<string> &func_args, int arg_flag) {
      if (!func_key_map_.empty()) {
        session_status_cache_[func_key_map_[func_name] + session_id] = {func_name, func_args, arg_flag};
      }
    }

    vector<TriggerFunctionMetadata> action_for_rerun(string &session_id) {
      if (!func_key_map_.empty()) {
        vector<TriggerFunctionMetadata> rerun_funcs;
        for (auto &key : key_set_) {
          if (session_status_cache_.find(key + session_id) != session_status_cache_.end()){
            rerun_funcs.push_back(session_status_cache_[key + session_id]);
          }
        }
        return rerun_funcs;
      }
      else {
        return {};
      }
    }
    
    void set_rerun_metadata(vector<RerunMetadata> &rerun_metadata) {
      for (auto &meta : rerun_metadata) func_key_map_[meta.source_func_] = meta.source_key_;
    }

    string dump_pritimive(){
      BySetPrimitive primitive;
      primitive.set_function(target_function_);
      for (auto &key : key_set_){
        primitive.add_key_set(key);
      }

      string prm_serialized;
      primitive.SerializeToString(&prm_serialized);
      return prm_serialized;
    }

    void clear(const string &session) {
      session_prepared_keys_map_.erase(session);
      session_status_cache_.erase(session);
    }

  private:
    set<Key> key_set_;
    map<Session, set<Key>> session_prepared_keys_map_;
    map<string, Key> func_key_map_;
    map<string, TriggerFunctionMetadata> session_status_cache_;
};


const string group_delimiter = ":";
const string control_group_prefix = "ctrl";
/**
 * currently this triggrt is session-agnostic
 * TODO add session support in future
 */ 
class DynamicGroupTrigger : public Trigger {
  public:
    DynamicGroupTrigger(const string &function_name, const string &trigger_name, const set<string> &control_group): 
      Trigger(function_name, trigger_name, PrimitiveType::DYNAMIC_GROUP),
      control_group_(control_group) {}

    vector<TriggerAction> action_for_new_object(const BucketKey &bucket_key) {
      vector<TriggerAction> actions;

      string group = bucket_key.key_.substr(0, bucket_key.key_.find(group_delimiter));
      if (group == control_group_prefix) {
        if (control_group_.find(bucket_key.key_) != control_group_.end()){
          cur_control_keys_.insert(bucket_key.key_);
        }
        if (cur_control_keys_.size() == control_group_.size()) {
          for (auto &group_keys : data_group_keys_map_){
            TriggerAction action;
            action.function_ = target_function_;
            for (auto &key : group_keys.second){
              // TODO session support
              action.session_keys_.push_back(std::make_pair(bucket_key.session_, key));
            }
            actions.push_back(action);
          }
          data_group_keys_map_.clear();
          cur_control_keys_.clear();
        }
      }
      else {
        data_group_keys_map_[group].insert(bucket_key.key_);
      }
      return actions;
    }

    string dump_pritimive(){
      DynamicGroupPrimitive primitive;
      primitive.set_function(target_function_);
      for (auto &control_key : control_group_){
        primitive.add_control_group(control_key);
      }

      string prm_serialized;
      primitive.SerializeToString(&prm_serialized);
      return prm_serialized;
    }

    void clear(const string &session) {
      data_group_keys_map_.clear();
      cur_control_keys_.clear();
    }

  private:
    map<string, set<Key>> data_group_keys_map_;
    set<Key> control_group_;
    set<Key> cur_control_keys_;
};

class RedundantTrigger : public Trigger {
  public:
    RedundantTrigger(const string &function_name, const string &trigger_name, unsigned k, unsigned n): 
      Trigger(function_name, trigger_name, PrimitiveType::REDUNDANT), k_(k), n_(n), count_(0) {}

    vector<TriggerAction> action_for_new_object(const BucketKey &bucket_key) {
      vector<TriggerAction> actions;
      count_++;
      if (count_ <= k_) {
        buffer_.push_back(std::make_pair(bucket_key.session_, bucket_key.key_));
        if (count_ == k_) {
          actions.push_back({target_function_, buffer_});
          buffer_.clear();
        }
      }
      else if (count_ >= n_) {
        // refresh
        count_ = 0;
      }
      // in other cases we just drop the data
      
      return actions;
    }

    string dump_pritimive(){
      RedundantPrimitive primitive;
      primitive.set_function(target_function_);
      primitive.set_k(k_);
      primitive.set_n(n_);

      string prm_serialized;
      primitive.SerializeToString(&prm_serialized);
      return prm_serialized;
    }

    void clear(const string &session) {
      count_ += k_;
      buffer_.clear();
    }

  private:
    unsigned k_;
    unsigned n_;
    unsigned count_;
    vector<pair<Session, Key>> buffer_;
};

class ByTimeTrigger : public Trigger {
  public:
    ByTimeTrigger(const string &function_name, const string &trigger_name, unsigned time_window): 
      Trigger(function_name, trigger_name, PrimitiveType::BY_TIME), time_window_(time_window) {
        last_trigger_stamp_ = std::chrono::system_clock::now();
      }

    vector<TriggerAction> action_for_new_object(const BucketKey &bucket_key) {
      buffer_.push_back(std::make_pair(bucket_key.session_, bucket_key.key_));

      // In most cases, the streaming data come with high frequency, thus we just check the timestamp upon data arrival
      // Consider periodically trigger in future 
      vector<TriggerAction> actions;
      auto cur_stamp = std::chrono::system_clock::now();
      if (std::chrono::duration_cast<std::chrono::milliseconds>(cur_stamp - last_trigger_stamp_).count() >= time_window_){
        actions.push_back({target_function_, buffer_});
        last_trigger_stamp_ = cur_stamp;
        buffer_.clear();
      }
      return actions;
    }

    string dump_pritimive(){
      ByTimePrimitive primitive;
      primitive.set_function(target_function_);
      primitive.set_time_window(time_window_);

      string prm_serialized;
      primitive.SerializeToString(&prm_serialized);
      return prm_serialized;
    }

    void clear(const string &session) {buffer_.clear();}

  private:
    unsigned time_window_;
    vector<pair<Session, Key>> buffer_;
    TimePoint last_trigger_stamp_;
};

/**
Generate trigger pointer from serialized data
*/
inline TriggerPointer gen_trigger_pointer(PrimitiveType type, string &trigger_name, const string &serialized){
  if (type == PrimitiveType::IMMEDIATE) {
    ImmediatePrimitive primitive;
    primitive.ParseFromString(serialized);
    return std::make_shared<ImmediateTrigger>(primitive.function(), trigger_name);
  }
  else if (type == PrimitiveType::BY_NAME) {
    ByNamePrimitive primitive;
    primitive.ParseFromString(serialized);
    return std::make_shared<ByNameTrigger>(primitive.function(), trigger_name, primitive.key_name());
  }
  else if (type == PrimitiveType::BY_BATCH_SIZE) {
    ByBatchSizePrimitive primitive;
    primitive.ParseFromString(serialized);
    return std::make_shared<ByBatchSizeTrigger>(primitive.function(), trigger_name, primitive.count());
  }
  else if (type == PrimitiveType::BY_SET) {
    BySetPrimitive primitive;
    primitive.ParseFromString(serialized);
    set<Key> key_set;
    for (auto &key : primitive.key_set()) {
      key_set.insert(key);
    }
    return std::make_shared<BySetTrigger>(primitive.function(), trigger_name, key_set);
  }
  else if (type == PrimitiveType::DYNAMIC_GROUP) {
    DynamicGroupPrimitive primitive;
    primitive.ParseFromString(serialized);
    set<Key> control_group;
    for (auto &key : primitive.control_group()) {
      control_group.insert(key);
    }
    return std::make_shared<DynamicGroupTrigger>(primitive.function(), trigger_name, control_group);
  }
  else if (type == PrimitiveType::REDUNDANT) {
    RedundantPrimitive primitive;
    primitive.ParseFromString(serialized);
    return std::make_shared<RedundantTrigger>(primitive.function(), trigger_name, primitive.k(), primitive.n());
  }
  else if (type == PrimitiveType::BY_TIME) {
    ByTimePrimitive primitive;
    primitive.ParseFromString(serialized);
    unsigned time_window = primitive.time_window();
    return std::make_shared<ByTimeTrigger>(primitive.function(), trigger_name, time_window);
  } 
  else {
    return nullptr;
  }
}

#endif