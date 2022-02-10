#include "coord_handlers.hpp"
#include "yaml-cpp/yaml.h"

ZmqUtil zmq_util;
ZmqUtilInterface *kZmqUtil = &zmq_util;

// prepare the zmq context
zmq::context_t context(1);

unsigned CoordReportThreshold = 120;
unsigned seed = time(NULL);
unsigned io_thread_num;

Address managementAddress;

void update_address(
    const BucketKey &bucket_key,
    const Address &addr, 
    map<Bucket, map<Key, set<Address>>> &bucket_key_address_map,
    logger log) {
  // it should be identical, so we do not check overriding
  bucket_key_address_map[bucket_key.bucket_][bucket_key.key_].insert(addr);
}

GetAddressResult get_address(
    const BucketKey &bucket_key,
    map<Bucket, map<Key, set<Address>>> &bucket_key_address_map,
    logger log) {
  GetAddressResult result;
  // we return the address if the bucket_key exists
  if (bucket_key_address_map.find(bucket_key.bucket_) != bucket_key_address_map.end() && 
      bucket_key_address_map[bucket_key.bucket_].find(bucket_key.key_) != bucket_key_address_map[bucket_key.bucket_].end()) {
    result.error_ = KVSError::SUCCESS;
    result.addr_ = bucket_key_address_map[bucket_key.bucket_][bucket_key.key_];
  }
  else {
    result.error_ = KVSError::KEY_NE;
  }
  return result;
}

void run(Address public_ip, Address private_ip, unsigned thread_id) {
  string log_file = "log_coordinator_" + std::to_string(thread_id) + ".txt";
  string log_name = "log_coordinator_" + std::to_string(thread_id);
  auto log = spdlog::basic_logger_mt(log_name, log_file, true);
  log->flush_on(spdlog::level::info);

  HandlerThread ht = HandlerThread(private_ip, thread_id);
  OperationThread ot = OperationThread(private_ip, thread_id);

  seed += thread_id;

  auto res = context.setctxopt(ZMQ_MAX_SOCKETS, kMaxSocketNumber);
  if (res == 0) {
    log->info("Successfully set max socket number to {}", kMaxSocketNumber);
  } else {
    log->error("E: socket error number {} ({})", errno, zmq_strerror(errno));
  }

  SocketCache pushers(&context, ZMQ_PUSH);

  map<Bucket, map<Key, set<Address>>> bucket_key_address_map;
  map<string, string> bucket_key_val_map;
  map<string, set<Address>> bucket_node_map;

  map<Bucket, vector<TriggerPointer>> bucket_triggers_map;
  map<string, set<Bucket>> app_buckets_map;
  map<Bucket, string> bucket_app_map;

  map<Address, NodeStatus> node_status_map;

  zmq::socket_t notify_handler_puller(context, ZMQ_PULL);
  notify_handler_puller.bind(ht.notify_handler_bind_address());
  std::cout << "Notifying handler binded" << std::endl;

  zmq::socket_t query_handler_puller(context, ZMQ_PULL);
  query_handler_puller.bind(ht.key_query_handler_bind_address());
  std::cout << "Querying handler binded" << std::endl;

  zmq::socket_t bucket_op_handler_puller(context, ZMQ_PULL);
  bucket_op_handler_puller.bind(ot.bucket_op_bind_address());
  std::cout << "Bucket operation handler binded" << std::endl;

  zmq::socket_t trigger_op_handler_puller(context, ZMQ_PULL);
  trigger_op_handler_puller.bind(ot.trigger_op_bind_address());
  std::cout << "Trigger operation handler binded" << std::endl;

  zmq::socket_t update_handler_puller(context, ZMQ_PULL);
  update_handler_puller.bind(ht.update_handler_bind_address());
  std::cout << "Update status handler binded" << std::endl;

  // socket for global scheduler
  zmq::socket_t func_create_socket(context, ZMQ_REP);
  func_create_socket.bind(ot.func_create_bind_address());
  std::cout << "func create socket binded" << std::endl;

  zmq::socket_t app_regist_socket(context, ZMQ_PULL);
  app_regist_socket.bind(ot.app_regist_bind_address());
  std::cout << "app regist socket binded" << std::endl;

  zmq::socket_t func_call_socket(context, ZMQ_PULL);
  func_call_socket.bind(ot.func_call_bind_address());
  std::cout << "func call socket binded" << std::endl;

  zmq::socket_t forward_func_socket(context, ZMQ_PULL);
  forward_func_socket.bind(ht.forward_func_bind_address());
  std::cout << "forward func socket binded" << std::endl;

  vector<zmq::pollitem_t> pollitems = {
    {static_cast<void *>(notify_handler_puller), 0, ZMQ_POLLIN, 0},
    {static_cast<void *>(query_handler_puller), 0, ZMQ_POLLIN, 0},
    {static_cast<void *>(bucket_op_handler_puller), 0, ZMQ_POLLIN, 0},
    {static_cast<void *>(trigger_op_handler_puller), 0, ZMQ_POLLIN, 0},
    {static_cast<void *>(update_handler_puller), 0, ZMQ_POLLIN, 0},
    {static_cast<void *>(func_create_socket), 0, ZMQ_POLLIN, 0},
    {static_cast<void *>(app_regist_socket), 0, ZMQ_POLLIN, 0},
    {static_cast<void *>(func_call_socket), 0, ZMQ_POLLIN, 0},
    {static_cast<void *>(forward_func_socket), 0, ZMQ_POLLIN, 0},
  };

  // sync coord with management
  CoordSync msg;
  msg.set_public_ip(public_ip);
  msg.set_private_ip(private_ip);
  msg.set_thread_id(thread_id);

  string msg_serialized;
  msg.SerializeToString(&msg_serialized);
  kZmqUtil->send_string(msg_serialized, &pushers[managementAddress]);

  auto report_start = std::chrono::system_clock::now();
  auto report_end = std::chrono::system_clock::now();

  int notify_count = 0;
  int query_count = 0;
  int call_count = 0;

  while (true) {
    // block until a requested event if timeout -1
    // return intermediately if timeout 0
    kZmqUtil->poll(0, &pollitems);

    // handle put notifying request
    if (pollitems[0].revents & ZMQ_POLLIN) {
      string serialized = kZmqUtil->recv_string(&notify_handler_puller);
      notify_handler(log, serialized, pushers, bucket_key_address_map, bucket_triggers_map, 
                    bucket_app_map, bucket_key_val_map, bucket_node_map, node_status_map);
      notify_count++;
    }

    // handle key querying request
    if (pollitems[1].revents & ZMQ_POLLIN) {
      string serialized = kZmqUtil->recv_string(&query_handler_puller);
      query_handler(log, serialized, pushers, bucket_key_address_map);
      query_count++;
    }

    // handle bucket operation request
    if (pollitems[2].revents & ZMQ_POLLIN) {
      string serialized = kZmqUtil->recv_string(&bucket_op_handler_puller);
      bucket_op_handler(log, serialized, pushers, bucket_triggers_map, app_buckets_map, bucket_app_map);
    }

    // handle trigger operation request
    if (pollitems[3].revents & ZMQ_POLLIN) {
      string serialized = kZmqUtil->recv_string(&trigger_op_handler_puller);
      trigger_op_handler(log, serialized, private_ip, thread_id, pushers, bucket_triggers_map, bucket_app_map, node_status_map);
    }

    // handle status update from local scheduler
    if (pollitems[4].revents & ZMQ_POLLIN) {
      string serialized = kZmqUtil->recv_string(&update_handler_puller);
      UpdateStatusMessage msg;
      msg.ParseFromString(serialized);
      
      string ip = msg.ip();
      int avail_executors = msg.avail_executors();
      set<string> functions;
      for (const auto &func : msg.functions()){
        functions.insert(func);
      }
      if (node_status_map.find(ip) != node_status_map.end()){
        node_status_map[ip].update_status(avail_executors, functions);
      }
      else {
        NodeStatus node_status;
        node_status.avail_executors_ = avail_executors;
        node_status.functions_ = functions;
        node_status.tp_ = std::chrono::system_clock::now();
        node_status_map[ip] = node_status;
      }
    }

    // handle function creation request
    if (pollitems[5].revents & ZMQ_POLLIN) {
      string serialized = kZmqUtil->recv_string(&func_create_socket);
    }

    // handle app registration request
    if (pollitems[6].revents & ZMQ_POLLIN) {
      string serialized = kZmqUtil->recv_string(&app_regist_socket);
      app_register_handler(log, serialized, private_ip, thread_id, pushers, bucket_triggers_map, app_buckets_map, bucket_app_map, node_status_map);
    }
    
    // handle function call
    if (pollitems[7].revents & ZMQ_POLLIN) {
      string serialized = kZmqUtil->recv_string(&func_call_socket);
      func_call_handler(log, serialized, pushers, node_status_map);
      call_count++;
    }

    // forward function call
    if (pollitems[8].revents & ZMQ_POLLIN) {
      string serialized = kZmqUtil->recv_string(&forward_func_socket);
      func_call_handler(log, serialized, pushers, node_status_map);
      call_count++;
    }

    report_end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(report_end - report_start).count();
    if (duration >= CoordReportThreshold) {
      report_start = std::chrono::system_clock::now();
      // TODO remote out-of-data node status
      log->info("Coordinator report. notify_count: {}, query_count: {}, call_count: {}", notify_count, query_count, call_count);

    }

  }
}

int main(int argc, char *argv[]) {
  if (argc != 1) {
    std::cerr << "Usage: " << argv[0] << std::endl;
    return 1;
  }

  YAML::Node conf = YAML::LoadFile("conf/config.yml");
  
  unsigned threads = conf["coord_threads"].as<unsigned>();
  std::cout << "Thread count " << threads << std::endl;

  io_thread_num = conf["io_threads"].as<unsigned>();

  managementAddress = "tcp://" + conf["manager"].as<string>() + ":" + std::to_string(coordSyncPort);

  YAML::Node ip = conf["ip"];
  Address private_ip = ip["private"].as<Address>();
  Address public_ip = ip["public"].as<Address>();

  vector<std::thread> coord_worker_threads;

  for (unsigned thread_id = 1; thread_id < threads; thread_id++) {
    coord_worker_threads.push_back(std::thread(run, public_ip, private_ip, thread_id));
  }
  
  run(public_ip, private_ip, 0);
}
