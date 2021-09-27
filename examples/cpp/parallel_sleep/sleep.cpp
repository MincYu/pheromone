#include <iostream>
#include <chrono>
#include <cstring>
#include <thread>
#include "cpp_function.hpp"

extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        auto start_t = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        // std::cout << "sleep function start: " << start_t << std::endl;

        auto obj = library->create_object("b_parallel_join", library->gen_unique_key(), 13);
        auto val = static_cast<char *>(obj->get_value());
        memcpy(val, std::to_string(start_t).c_str(), 13);
        library->send_object(obj);
        return 0;
    }
}