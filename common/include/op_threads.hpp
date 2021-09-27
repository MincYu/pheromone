#ifndef INCLUDE_OP_THREADS_HPP_
#define INCLUDE_OP_THREADS_HPP_

#include "anna_threads.hpp"

// global scheduler ports
const unsigned funcCreatePort = 5000;

const unsigned appRegistPort = 5020;

const unsigned funcCallPort = 5050;

const unsigned coordSyncPort = 6001;

const unsigned coordQueryPort = 6002;

const unsigned oBucketPort = 7800;

const unsigned oTriggerPort = 7900;

class OperationThread {
  Address ip_;
  Address ip_base_;
  unsigned tid_;

 public:
  OperationThread(Address ip, unsigned tid) :
      ip_(ip),
      ip_base_("tcp://" + ip_ + ":"),
      tid_(tid) {}

  Address ip() const { return ip_; }

  unsigned tid() const { return tid_; }

  Address bucket_op_bind_address() const { return kBindBase + std::to_string(tid_ + oBucketPort); }

  Address bucket_op_connect_address() const { return ip_base_ + std::to_string(tid_ + oBucketPort); }

  Address trigger_op_bind_address() const { return kBindBase + std::to_string(tid_ + oTriggerPort); }

  Address trigger_op_connect_address() const { return ip_base_ + std::to_string(tid_ + oTriggerPort); }

  Address func_create_bind_address() const { return kBindBase + std::to_string(tid_ + funcCreatePort); }
  Address app_regist_bind_address() const { return kBindBase + std::to_string(tid_ + appRegistPort); }
  Address func_call_bind_address() const { return kBindBase + std::to_string(tid_ + funcCallPort); }

};

#endif  // INCLUDE_OP_THREADS_HPP_
