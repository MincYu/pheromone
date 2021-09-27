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

    /**
     * Action to take given the new object
     * it includes the function name if it decides to trigger the execution,
     * or a empty string if no action is needed
     */ 
    virtual vector<TriggerAction> action_for_new_object(const BucketKey &bucket_key) = 0;
    virtual string dump_pritimive() = 0;
    virtual void clear() = 0;

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
      TriggerAction action = {target_function_, {std::make_pair(bucket_key.session_, bucket_key.key_)}};
      return vector<TriggerAction>{action};
    }

    string dump_pritimive(){
      ImmediatePrimitive primitive;
      primitive.set_function(target_function_);

      string prm_serialized;
      primitive.SerializeToString(&prm_serialized);
      return prm_serialized;
    }

    void clear() {};

};

class ByNameTrigger : public Trigger {
  public:
    ByNameTrigger(const string &function_name, const string &trigger_name, const string &key_name): 
      Trigger(function_name, trigger_name, PrimitiveType::BY_NAME),
      key_name_(key_name) {}

    vector<TriggerAction> action_for_new_object(const BucketKey &bucket_key) {
      vector<TriggerAction> actions;
      if (bucket_key.key_ == key_name_) {
        TriggerAction action;
        action.function_ = target_function_;
        action.session_keys_ = {std::make_pair(bucket_key.session_, bucket_key.key_)};
        actions.push_back(action);
      }

      return actions;
    }

    string dump_pritimive(){
      ByNamePrimitive primitive;
      primitive.set_function(target_function_);
      primitive.set_key_name(key_name_);

      string prm_serialized;
      primitive.SerializeToString(&prm_serialized);
      return prm_serialized;
    }
  
    void clear() {};

  private:
    string key_name_;

};

class ByBatchSizeTrigger : public Trigger {
  public:
    ByBatchSizeTrigger(const string &function_name, const string &trigger_name, int count): 
      Trigger(function_name, trigger_name, PrimitiveType::BY_BATCH_SIZE),
      count_(count) {}

    vector<TriggerAction> action_for_new_object(const BucketKey &bucket_key) {
      vector<TriggerAction> actions;
      
      buffer_.push_back(std::make_pair(bucket_key.session_, bucket_key.key_));
      if (buffer_.size() >= count_) {
        TriggerAction action;
        action.function_ = target_function_;
        action.session_keys_ = std::move(buffer_);
        actions.push_back(action);
        clear();
      }
      
      return actions;
    }
  
    string dump_pritimive(){
      ByBatchSizePrimitive primitive;
      primitive.set_function(target_function_);
      primitive.set_count(count_);

      string prm_serialized;
      primitive.SerializeToString(&prm_serialized);
      return prm_serialized;
    }

    void clear() {
      buffer_.clear();
    };

  private:
    int count_;
    vector<pair<Session, Key>> buffer_;
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
        if (session_prepared_keys_map_[bucket_key.session_].size() == key_set_.size()) {
          TriggerAction action;
          action.function_ = target_function_;
          for (auto &key: key_set_){
            action.session_keys_.push_back(std::make_pair(bucket_key.session_, key));
          }
          actions.push_back(action);
          session_prepared_keys_map_.erase(bucket_key.session_);
        }
        
        // if (bucket_key.vt_ == ValueType::NORMAL) {
        //   prepared_keys_.insert(bucket_key.key_);
        //   // the keys are ready
        //   if (prepared_keys_.size() == key_set_.size()) {
        //     action.function_ = target_function_;
        //     for (auto &key: key_set_){
        //       action.session_keys_.push_back(std::make_pair("", key));
        //     }
        //     prepared_keys_.clear();
        //   }
        //   else {
        //     action.function_ = "";
        //   }
        // }
        // else if (bucket_key.vt_ == ValueType::SESSION) {
        //   session_prepared_keys_map_[bucket_key.session_].insert(bucket_key.key_);
        //   if (session_prepared_keys_map_[bucket_key.session_].size() == key_set_.size()) {
        //     action.function_ = target_function_;
        //     for (auto &key: key_set_){
        //       action.session_keys_.push_back(std::make_pair(bucket_key.session_, key));
        //     }
        //     session_prepared_keys_map_.erase(bucket_key.session_);
        //   }
        //   else {
        //     action.function_ = "";
        //   }
        // }
      }
      return actions;
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

    void clear() {
      prepared_keys_.clear();
      session_prepared_keys_map_.clear();
    };

  private:
    set<Key> key_set_;

    // The two are for keeping keys that have been prepared.
    // Note that only one of them would be used, which is based on ValueType of bucket
    set<Key> prepared_keys_;
    map<Session, set<Key>> session_prepared_keys_map_;
};


const string group_delimiter = ":";
const string control_group_prefix = "ctrl";
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
              action.session_keys_.push_back(std::make_pair("", key));
            }
            actions.push_back(action);
          }
          clear();
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

    void clear() {
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
        buffer_.push_back(std::make_pair("", bucket_key.key_));
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

    void clear() {
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
      buffer_.push_back(std::make_pair("", bucket_key.key_));

      // In most cases, the streaming data come with high frequency, thus we just check the timestamp upon data arrival
      // Consider periodically trigger in future 
      vector<TriggerAction> actions;
      auto cur_stamp = std::chrono::system_clock::now();
      if (std::chrono::duration_cast<std::chrono::milliseconds>(cur_stamp - last_trigger_stamp_).count() >= time_window_){
        actions.push_back({target_function_, buffer_});
        last_trigger_stamp_ = cur_stamp;
        clear();
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

    void clear() {buffer_.clear();}

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