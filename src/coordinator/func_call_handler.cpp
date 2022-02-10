#include "coord_handlers.hpp"

extern unsigned seed;
extern unsigned io_thread_num;

void func_call_handler(logger log, string &serialized, SocketCache &pushers,
                        map<Address, NodeStatus> &node_status_map){
  auto receive_req_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  FunctionCall call_msg;
  call_msg.ParseFromString(serialized);

  string app_name = call_msg.app_name();
  
  map<Address, string> scheduled_node_msg;
  auto req_num = call_msg.requests_size();
  string source = call_msg.source();

  int max_executors = 0;
  Address node_with_max_executors;

  string resp_address = call_msg.resp_address();

  if (call_msg.session_id().empty()){
    // a random session id in 16 bytes
    string session_id = gen_random(seed, 16); 
    call_msg.set_session_id(session_id);
    call_msg.SerializeToString(&serialized);
  }

  // string response_key = string();
  // if (!resp_address.empty()) {
  //   // synchronous requests
  //   response_key = call_msg.response_key();
  //   if (response_key.empty()){
  //     response_key = gen_random(seed, 16); // a random id with 16 bytes
  //     call_msg.set_response_key(response_key);
  //     call_msg.SerializeToString(&serialized);
  //   }
  // }

  // batch scheduling
  if (req_num > 1) {
    int total_executors = 0;
    vector<pair<Address, int>> avail_nodes;
    for (auto &pair : node_status_map){
      if (!source.empty() && source == pair.first){
        continue;
      }
      if (pair.second.avail_executors_ > 0){
        total_executors += pair.second.avail_executors_;
        avail_nodes.push_back(std::make_pair(pair.first, pair.second.avail_executors_));
      }
      if(pair.second.avail_executors_ > max_executors){
        max_executors = pair.second.avail_executors_;
        node_with_max_executors = pair.first;
      }
    }

    if (max_executors >= req_num) {
      scheduled_node_msg[node_with_max_executors] = serialized;
    }
    else {
      if (total_executors >= req_num) {
        vector<int> node_loads(avail_nodes.size());

        int node_idx = 0;
        for(int i = 0; i < req_num; i++){
          while (avail_nodes[node_idx].second <= 0){
            node_idx = (node_idx + 1) % avail_nodes.size();
          }
          node_loads[node_idx]++;
          avail_nodes[node_idx].second--;
          node_idx = (node_idx + 1) % avail_nodes.size();
        }

        FunctionCall routed_call;
        routed_call.set_app_name(app_name);
        routed_call.set_resp_address(resp_address);
        routed_call.set_sync_data_status(true);
        int avail_node_index = 0;
        int cur_load = 0;
        for (auto &req : call_msg.requests()){
          auto func_req = routed_call.add_requests();
          func_req->CopyFrom(req);
          cur_load++;
          if (cur_load == node_loads[avail_node_index]){
            string part_serialized;
            routed_call.SerializeToString(&part_serialized);
            scheduled_node_msg[avail_nodes[avail_node_index].first] = part_serialized;
            avail_node_index++;
            cur_load = 0;
            routed_call.clear_requests();
          }
        }

      }
    }
  }
  else {
    for (auto &pair : node_status_map){
      if (!source.empty() && source == pair.first){
        continue;
      }
      if(pair.second.avail_executors_ > max_executors){
        max_executors = pair.second.avail_executors_;
        node_with_max_executors = pair.first;
      }
    }
    if (max_executors > 0) {
      scheduled_node_msg[node_with_max_executors] = serialized;
    }
    else {
      // work around for fully utilizing executors
      if (node_status_map.size() > 0) {
        auto it = node_status_map.begin();
        std::advance(it, rand_r(&seed) % node_status_map.size());
        scheduled_node_msg[it->first] = serialized;
      }
    }
  }

  if (scheduled_node_msg.empty()){
    auto return_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    log->info("No worker for app function call {}. req: {}, recv: {}, return: {}", app_name, req_num, receive_req_stamp, return_stamp);
    if (!resp_address.empty()){
      FunctionCallResponse resp;
      resp.set_app_name(app_name);
      resp.set_request_id(call_msg.request_id());
      resp.set_error_no(1);

      string resp_serialized;
      resp.SerializeToString(&resp_serialized);
      kZmqUtil->send_string(resp_serialized, &pushers[resp_address]);
    }
  }
  else{
    auto scheduled_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    for (auto &node_msg : scheduled_node_msg){
      string func_exec_addr = get_func_exec_address(node_msg.first, rand_r(&seed) % io_thread_num);
      kZmqUtil->send_string(node_msg.second, &pushers[func_exec_addr]);
    }
    std::cout << "App function call "<< app_name << ". recv: " << receive_req_stamp << ", scheduled: " << scheduled_stamp << std::endl;
    log->info("App function call {}. recv: {}, scheduled {}.", app_name, receive_req_stamp, scheduled_stamp);
  }
}
