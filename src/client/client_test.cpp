#include <signal.h>
#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <cstddef>

#include "operation.pb.h"
#include "types.hpp"
#include "common.hpp"
#include "requests.hpp"
#include "kvs_threads.hpp"
#include "trigger.hpp"
#include "yaml-cpp/yaml.h"

template <class T>
using vector = std::vector<T>;

ZmqUtil zmq_util;
ZmqUtilInterface *kZmqUtil = &zmq_util;

inline pair<string, unsigned> get_coord(string &app_name, map<string, pair<string, unsigned>> &app_coord_map, zmq::socket_t &mngt_socket){
  if (app_coord_map.find(app_name) == app_coord_map.end()){
    CoordQuery query;
    query.set_application(app_name);

    string serialized;
    query.SerializeToString(&serialized);
    kZmqUtil->send_string(serialized, &mngt_socket);

    string serialized_resp = kZmqUtil->recv_string(&mngt_socket);
    CoordResponse resp;
    resp.ParseFromString(serialized_resp);
    app_coord_map[app_name] = std::make_pair(resp.coord_ip(), resp.thread_id());
  }
  return app_coord_map[app_name];
}

int main(int argc, char *argv[]) {
  // read the YAML conf
  YAML::Node conf = YAML::LoadFile("conf/config.yml");

  Address management = conf["management"].as<Address>();
  Address ip = conf["ip"].as<Address>();

  unsigned app_num = conf["app"].as<unsigned>();
  unsigned req_num = conf["req"].as<unsigned>();
  // unsigned sleep_in_micro = conf["sleep"].as<unsigned>();
  unsigned seed_ = time(NULL);

  map<string, pair<string, unsigned>> app_coord_map;
  zmq::context_t context(1);
  SocketCache pushers(&context, ZMQ_PUSH);

  zmq::socket_t mngt_socket(context, ZMQ_REQ);
  mngt_socket.connect("tcp://" + management + ":" + std::to_string(6002));

  for (int i = 0; i < app_num; i++){
    auto app_name = "app_" + std::to_string(i);
    auto func_name = "empty";
    auto coord_thread = get_coord(app_name, app_coord_map, mngt_socket);
    AppRegistration msg;
    msg.set_app_name(app_name);
    msg.add_functions(func_name);
    
    string serialized;
    msg.SerializeToString(&serialized);
    kZmqUtil->send_string(serialized, &pushers["tcp://" + coord_thread.first + ":" + std::to_string(coord_thread.second +  5020)]);
  }

  std::cout << "app registered " << std::endl;

  auto start_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
  FunctionCall call;
  auto req = call.add_requests();
  req->set_name("empty");

  // int batch_size = 10000; // req_num in batch
  // unsigned req_num = 0;
  // while (true) {
    for (int i = 0; i < req_num; i++){
      auto app_index = rand_r(&seed_) % app_num;
      auto app_name = "app_" + std::to_string(app_index);
      auto coord_thread = get_coord(app_name, app_coord_map, mngt_socket);

      call.set_app_name(app_name);

      string serialized;
      call.SerializeToString(&serialized);
      kZmqUtil->send_string(serialized, &pushers["tcp://" + coord_thread.first + ":" + std::to_string(coord_thread.second +  5050)]);
      // if (sleep_in_micro > 0 ){
      //   std::this_thread::sleep_for(std::chrono::microseconds(sleep_in_micro));
      // }
    }

    auto end_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();

    // req_num += batch_size;
  //   if (end_stamp - start_stamp > timeout * 1000000) {
  //     break;
  //   }
  // }

  std::cout << "app: " << app_num << ", req: " << req_num << ", elasped: " << end_stamp - start_stamp << std::endl;
}