#include "common.hpp"
#include "requests.hpp"
#include "types.hpp"
#include "op_threads.hpp"
#include "operation.pb.h"
#include "yaml-cpp/yaml.h"

ZmqUtil zmq_util;
ZmqUtilInterface *kZmqUtil = &zmq_util;

bool externalUser;

struct CoordIp {
  string public_ip_;
  string private_ip_;
  unsigned thread_id_;
  
  CoordIp(const string& public_ip, const string& private_ip, const unsigned& thread_id) : 
    public_ip_(public_ip), private_ip_(private_ip), thread_id_(thread_id) {};

  inline bool operator==(const CoordIp& other) const {
    return public_ip_ == other.public_ip_ && private_ip_ == other.private_ip_ && thread_id_ == other.thread_id_;
  };
};

namespace std {
  template<>
  struct hash<CoordIp> {
    inline size_t operator()(const CoordIp& coord) const {
        return coord.public_ip_.size() + coord.private_ip_.size() + coord.thread_id_;
    }
  };
}

void run(Address ip) {
  string log_file = "log_management.txt";
  string log_name = "log_management";
  auto log = spdlog::basic_logger_mt(log_name, log_file, true);
  log->flush_on(spdlog::level::info);

  // prepare the zmq context
  zmq::context_t context(1);

  map<string, pair<string, unsigned>> app_coord_map;
  set<CoordIp> coord_ips;
  unsigned seed = time(NULL);

  SocketCache pushers(&context, ZMQ_PUSH);

  // socket for global scheduler
  zmq::socket_t coord_sync_socket(context, ZMQ_PULL);
  coord_sync_socket.bind(kBindBase + std::to_string(coordSyncPort));
  std::cout << "coord sync socket binded" << std::endl;

  zmq::socket_t coord_query_socket(context, ZMQ_REP);
  coord_query_socket.bind(kBindBase + std::to_string(coordQueryPort));
  std::cout << "coord query socket binded" << std::endl;

  vector<zmq::pollitem_t> pollitems = {
    {static_cast<void *>(coord_sync_socket), 0, ZMQ_POLLIN, 0},
    {static_cast<void *>(coord_query_socket), 0, ZMQ_POLLIN, 0},
  };

  auto report_start = std::chrono::system_clock::now();
  auto report_end = std::chrono::system_clock::now();

  while (true) {
    kZmqUtil->poll(0, &pollitems);

    // coord sync 
    if (pollitems[0].revents & ZMQ_POLLIN) {
      string serialized = kZmqUtil->recv_string(&coord_sync_socket);
      CoordSync msg;
      msg.ParseFromString(serialized);
      CoordIp coord_ip(msg.public_ip(), msg.private_ip(), msg.thread_id());
      coord_ips.insert(coord_ip);
      log->info("Coord Sync. public_ip: {}, private_ip: {}, thread_id: {}", msg.public_ip(), msg.private_ip(), msg.thread_id());
    }

    // coord query
    if (pollitems[1].revents & ZMQ_POLLIN) {
      string serialized = kZmqUtil->recv_string(&coord_query_socket);
      CoordQuery query;
      query.ParseFromString(serialized);

      string app_name = query.application();
      if (app_coord_map.find(app_name) == app_coord_map.end()){
        // randomly pick one coordinator
        auto coord_ip = *(std::next(begin(coord_ips), rand_r(&seed) % coord_ips.size()));
        auto ip_to_send = externalUser ? coord_ip.public_ip_ : coord_ip.private_ip_;
        app_coord_map[app_name] = std::make_pair(ip_to_send, coord_ip.thread_id_);
      }

      CoordResponse resp;
      resp.set_request_id(query.request_id());
      resp.set_coord_ip(app_coord_map[app_name].first);
      resp.set_thread_id(app_coord_map[app_name].second);

      string resp_serialized;
      resp.SerializeToString(&resp_serialized);
      kZmqUtil->send_string(resp_serialized, &coord_query_socket);
      log->info("Coord Query. app_name: {}", app_name);
    }

  }
}

int main(int argc, char *argv[]) {
  if (argc != 1) {
    std::cerr << "Usage: " << argv[0] << std::endl;
    return 1;
  }

  YAML::Node conf = YAML::LoadFile("conf/config.yml");
  Address ip = conf["ip"].as<Address>();
  externalUser = conf["external"].as<unsigned>() == 1;
  run(ip);
}
