#ifndef INCLUDE_KVS_THREADS_HPP_
#define INCLUDE_KVS_THREADS_HPP_

#include "anna_threads.hpp"

const unsigned funcExecPort = 4300;

const unsigned internalFuncCallPort = 4350;

const unsigned dataAccessServerPort = 4400;

const unsigned dataAccessClientPort = 4450;

const unsigned forwardFuncPort = 4500;

const unsigned kRemoteResponsePort = 7500;

const unsigned kRemoteGetPort = 7550;

const unsigned kNotifyPort = 7600;

const unsigned kQueryPort = 7650;

const unsigned kNotifyHandlerPort = 7350;

const unsigned kQueryHandlerPort = 7400;

const unsigned TriggerUpdatePort = 7700;

const unsigned updateStatusHandlerPort = 7750;

const unsigned bucketQueryPort = 8150;
const unsigned bucketUpdatePort = 8250;

class KVSThread {
  Address ip_;
  Address ip_base_;
  unsigned tid_;

 public:
  KVSThread(Address ip, unsigned tid) :
      ip_(ip),
      ip_base_("tcp://" + ip_ + ":"),
      tid_(tid) {}

  Address ip() const { return ip_; }

  unsigned tid() const { return tid_; }

  Address local_get_bind_address() const { return "ipc:///requests/get"; }

  Address local_get_connect_address() const { return "ipc:///requests/get"; }

  Address local_put_bind_address() const { return "ipc:///requests/put"; }

  Address local_put_connect_address() const { return "ipc:///requests/put"; }

  Address remote_get_bind_address() const { return kBindBase + std::to_string(tid_ + kRemoteGetPort); }

  Address remote_get_connect_address() const { return ip_base_ + std::to_string(tid_ + kRemoteGetPort); }
};


class CommHelperThread {
  Address ip_;
  Address ip_base_;
  unsigned tid_;

 public:
  CommHelperThread(Address ip, unsigned tid) :
      ip_(ip),
      ip_base_("tcp://" + ip_ + ":"),
      tid_(tid) {}

  Address ip() const { return ip_; }

  unsigned tid() const { return tid_; }
  
  Address remote_response_bind_address() const { return kBindBase + std::to_string(tid_ + kRemoteResponsePort); }

  Address remote_response_connect_address() const { return ip_base_ + std::to_string(tid_ + kRemoteResponsePort); }

  Address key_query_bind_address() const { return kBindBase + std::to_string(tid_ + kQueryPort); }

  Address key_query_connect_address() const { return ip_base_ + std::to_string(tid_ + kQueryPort); }

  Address notify_put_bind_address() const { return kBindBase + std::to_string(tid_ + kNotifyPort); }

  Address notify_put_connect_address() const { return ip_base_ + std::to_string(tid_ + kNotifyPort); }

  Address trigger_update_bind_address() const { return kBindBase + std::to_string(tid_ + TriggerUpdatePort); }

  Address trigger_update_connect_address() const { return ip_base_ + std::to_string(tid_ + TriggerUpdatePort); }

  Address data_access_server_bind_address() const { return kBindBase + std::to_string(tid_ + dataAccessServerPort); }

  Address data_access_server_connect_address() const { return ip_base_ + std::to_string(tid_ + dataAccessServerPort); }

  Address data_access_client_bind_address() const { return kBindBase + std::to_string(tid_ + dataAccessClientPort); }

  Address data_access_client_connect_address() const { return ip_base_ + std::to_string(tid_ + dataAccessClientPort); }
};

class HandlerThread {
  Address ip_;
  Address ip_base_;
  unsigned tid_;

 public:
  HandlerThread() {}

  HandlerThread(Address ip, unsigned tid) :
      ip_(ip),
      tid_(tid),
      ip_base_("tcp://" + ip_ + ":") {}

  Address ip() const { return ip_; }

  unsigned tid() const { return tid_; }

  Address notify_handler_connect_address() const { return ip_base_ + std::to_string(tid_ + kNotifyHandlerPort);}

  Address notify_handler_bind_address() const { return kBindBase + std::to_string(tid_ + kNotifyHandlerPort);}

  Address key_query_handler_connect_address() const {return ip_base_ + std::to_string(tid_ + kQueryHandlerPort);}

  Address key_query_handler_bind_address() const { return kBindBase + std::to_string(tid_ + kQueryHandlerPort);}

  Address update_handler_bind_address() const { return kBindBase + std::to_string(tid_ + updateStatusHandlerPort); }

  Address update_handler_connect_address() const { return ip_base_ + std::to_string(tid_ + updateStatusHandlerPort); }
  
  Address forward_func_bind_address() const { return kBindBase + std::to_string(tid_ + forwardFuncPort); }

  Address forward_func_connect_address() const { return ip_base_ + std::to_string(tid_ + forwardFuncPort); }

  bool has_same_ip_tid(Address ip, unsigned tid) {return ip_ == ip && tid_ == tid;}
};

class ManagementServerThread {
  Address ip_;
  Address ip_base_;
  unsigned tid_;

 public:
  ManagementServerThread(Address ip, unsigned tid) :
      ip_(ip),
      ip_base_("tcp://" + ip_ + ":"),
      tid_(tid) {}

  Address ip() const { return ip_; }

  unsigned tid() const { return tid_; }
  
  Address bucket_query_connect_address() const {
    return ip_base_ + std::to_string(tid_ + bucketQueryPort);
  }

  Address bucket_query_bind_address() const {
    return kBindBase + std::to_string(tid_ + bucketQueryPort);
  }

  Address bucket_update_connect_address() const { 
    return ip_base_ + std::to_string(tid_ + bucketUpdatePort); 
  }

  Address bucket_update_bind_address() const {
    return kBindBase + std::to_string(tid_ + bucketUpdatePort);
  }
};

#endif  // INCLUDE_KVS_THREADS_HPP_
