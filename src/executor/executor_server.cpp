#include <signal.h>
#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <cstddef>
#include <atomic>
#include "libipc/ipc.h"
#include "libipc/shm.h"
#include "capo/random.hpp"
#include <dlfcn.h>

#include "function_lib.hpp"
#include "yaml-cpp/yaml.h"


std::atomic<bool> is_quit__{ false };

unsigned ExecutorTimerThreshold = 1000; // every second
unsigned RecvWaitTm;
string funcDir;

bool load_function(logger log, string &func_name, map<string, CppFunction> &name_func_map){
    auto start_t = std::chrono::system_clock::now();
    
    string lib_name = funcDir + func_name + ".so";
    void *lib_handle = dlopen(lib_name.c_str(), RTLD_LAZY);
    if (!lib_handle) {
        std::cerr << func_name << ".so load failed (" << dlerror() << ")" << std::endl;
        log->error("Load lib {}.so failed", func_name);
        return false;
    }

    auto lib_load_t = std::chrono::system_clock::now();
 
    char *error = 0;
    CppFunction func = (CppFunction) dlsym(lib_handle, "handle");
    if ((error = dlerror()) != NULL) {
        std::cerr << "get handle function failed (" << error << ")" << std::endl;
        log->error("Load handle function from {}.so failed: {}", func_name, error);
        return false;
    }
    auto func_load_t = std::chrono::system_clock::now();

    name_func_map[func_name] = func;
    auto lib_load_elasped = std::chrono::duration_cast<std::chrono::microseconds>(lib_load_t - start_t).count();
    auto func_load_elasped = std::chrono::duration_cast<std::chrono::microseconds>(func_load_t - lib_load_t).count();
    log->info("Loaded function {}. lib_load_elasped: {}, func_load_elasped: {}", func_name, lib_load_elasped, func_load_elasped);
    return true;
}

inline void update_status(unsigned thread_id, bool busy_flag){
  string req;
  req.push_back(static_cast<uint8_t>(thread_id + 1)); // we add 1 to avoid \0 in string
  req.push_back(busy_flag ? 6 : 7); // 6 busy ; 7 available

  while (!shared_chan.send(req)) {
      shared_chan.wait_for_recv(1);
  }
}

void run(Address ip, unsigned thread_id) {
  string log_file = "log_executor.txt";
  string log_name = "log_executor";
  auto log = spdlog::basic_logger_mt(log_name, log_file, true);
  log->flush_on(spdlog::level::info);

  UserLibraryInterface *user_lib = new UserLibrary(ip, thread_id);
  map<string, CppFunction> name_func_map;

  string chan_name = "ipc-" + std::to_string(thread_id);
  local_chan = new shm_chan_t{chan_name.c_str(), ipc::receiver};
  
  std::cout << "Running executor...\n";
  log->info("Running executor...");

  auto report_start = std::chrono::system_clock::now();
  auto report_end = std::chrono::system_clock::now();

  while (true){
    auto dd = local_chan->recv(RecvWaitTm);
    auto str = static_cast<char*>(dd.data());
    
    if (str != nullptr) {
      // function call
      if (str[0] == 1){
        auto recv_stamp = std::chrono::system_clock::now();
        update_status(thread_id, true);

        uint8_t arg_flag = str[1] - 1;
        uint8_t persist_output_flag = str[2];

        string msg(str + 3);
        vector<string> func_with_args;
        split(msg, '|', func_with_args);

        string session_id = func_with_args[0];
        func_with_args.erase(func_with_args.begin());
        
        string func_name = func_with_args[0];
        // int func_id = stoi(func_with_args[1]);
        // std::cout << "Received function call " << func_name << "\n" << std::flush;

        static_cast<UserLibrary*>(user_lib)->set_function_name(func_name);
        static_cast<UserLibrary*>(user_lib)->set_session_id(session_id);
        static_cast<UserLibrary*>(user_lib)->set_persist_flag(persist_output_flag);
        if (name_func_map.find(func_name) == name_func_map.end()){
          // read .so from shared memory dir
          if(!load_function(log, func_name, name_func_map)){
            log->error("Fail to execute function {} due to load error", func_name);
            update_status(thread_id, false);
            continue;
          }
        }

        int arg_size;
        char ** arg_values;
        if (arg_flag == 0){
          // We parse plain args with splitter
          arg_size = func_with_args.size() - 1;
          arg_values = new char*[arg_size];
          for (int i = 0; i < arg_size; i++){
            auto index_in_func_args = i + 1;
            auto arg_size_in_bytes = func_with_args[index_in_func_args].size();
            char * arg_v = new char[arg_size_in_bytes + 1];
            std::copy(func_with_args[index_in_func_args].begin(), func_with_args[index_in_func_args].end(), arg_v);
            arg_v[arg_size_in_bytes] = '\0';
            arg_values[i] = arg_v;
            static_cast<UserLibrary*>(user_lib)->add_arg_size(arg_size_in_bytes);
          }
        }
        else {
          if (func_with_args.size() % 3 == 1){
            // can be divied by 3, exclude the function name
            if (arg_flag == 1) {
              arg_size = func_with_args.size() / 3;
            }
            else if (arg_flag == 2) {
              arg_size = 2 * (func_with_args.size() - 1) / 3;
            }
            arg_values = new char*[arg_size];

            for (int i = 1; i < func_with_args.size(); i+=3){
              string key_name = func_with_args[i] + kDelimiter + func_with_args[i + 1];
              auto shm_obj_size = stoi(func_with_args[i + 2]);
              auto shm_id = ipc::shm::acquire(key_name.c_str(), shm_obj_size, ipc::shm::open);
              auto shm_ptr = static_cast<char*>(ipc::shm::get_mem(shm_id, nullptr));
              if (arg_flag == 1) {
                arg_values[i/3] = shm_ptr;
                static_cast<UserLibrary*>(user_lib)->add_arg_size(shm_obj_size);
              }
              else if (arg_flag == 2) {
                int index = 2 * (i - 1) / 3;
                auto arg_size_in_bytes = func_with_args[i + 1].size();
                char * key_name_chars = new char[arg_size_in_bytes + 1];
                std::copy(func_with_args[i + 1].begin(), func_with_args[i + 1].end(), key_name_chars);
                key_name_chars[arg_size_in_bytes] = '\0';
                arg_values[index] = key_name_chars;
                arg_values[index + 1] = shm_ptr;
                static_cast<UserLibrary*>(user_lib)->add_arg_size(arg_size_in_bytes);
                static_cast<UserLibrary*>(user_lib)->add_arg_size(shm_obj_size);
              }
            }
          }
          else{
            log->error("Function {} cannot parse the shared memory args", func_name);
            update_status(thread_id, false);
            continue;
          }
        }
        
        auto recv_time = std::chrono::duration_cast<std::chrono::microseconds>(recv_stamp.time_since_epoch()).count();
        auto parse_stamp = std::chrono::system_clock::now();
        auto parse_time = std::chrono::duration_cast<std::chrono::microseconds>(parse_stamp.time_since_epoch()).count();

        log->info("Executing {} arg_size: {}. recv: {}, parse: {}", func_name, arg_size, recv_time, parse_time);

        int exit_signal = 1;

        try{
          exit_signal = name_func_map[func_name](user_lib, arg_size, arg_values);
        }
        catch (const std::overflow_error& e){
          std::cerr << "Function " << func_name << " throws overflow error " << e.what() << std::endl;
        }
        catch (const std::runtime_error& e){
          std::cerr << "Function " << func_name << " throws runtime error " << e.what() << std::endl;
        }
        catch (const std::exception& e){
          std::cerr << "Function " << func_name << " throws exception " << e.what() << std::endl;
        }
        catch (...){
          std::cerr << "Function " << func_name << " throws other type" << std::endl;
        }

        if (exit_signal != 0){
          std::cerr << "Function " << func_name << " exits with error " << exit_signal << std::endl;
          log->warn("Function {} exits with error {}", func_name, exit_signal);
        }
        auto execute_stamp = std::chrono::system_clock::now();
        static_cast<UserLibrary*>(user_lib)->clear_session();

        update_status(thread_id, false);

        auto execute_time = std::chrono::duration_cast<std::chrono::microseconds>(execute_stamp.time_since_epoch()).count();
        log->info("Executed {} at: {}", func_name, execute_time);
        // std::cout << "Executed function " << func_name << "\n" << std::flush;

      }
    }
    report_end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(report_end - report_start).count();

    if (duration >= ExecutorTimerThreshold) {
      report_start = std::chrono::system_clock::now();
      update_status(thread_id, false);
      // log->info("Executer {} report.", thread_id);
    }
   
  }
  std::cout << __func__ << ": quit...\n";
}

int main(int argc, char *argv[]) {
    auto exit = [](int) {
        is_quit__.store(true, std::memory_order_release);
        shared_chan.disconnect();
        local_chan->disconnect();
        std::cout << "Exit with env cleared\n" << std::flush;
    };
    ::signal(SIGINT  , exit);
    ::signal(SIGABRT , exit);
    ::signal(SIGSEGV , exit);
    ::signal(SIGTERM , exit);
    ::signal(SIGHUP  , exit);

    // read the YAML conf
    YAML::Node conf = YAML::LoadFile("conf/config.yml");
    std::cout << "Read file config.yml" << std::endl;

    funcDir = conf["func_dir"].as<string>();

    if (YAML::Node wait_tm = conf["wait"]) {
      RecvWaitTm = wait_tm.as<unsigned>();
    } 
    else {
      RecvWaitTm = 0;
    }

    Address ip = conf["ip"].as<Address>();
    unsigned thread_id = conf["thread_id"].as<unsigned>();

    run(ip, thread_id);
}


