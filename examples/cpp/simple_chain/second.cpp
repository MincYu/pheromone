#include <iostream>
#include <chrono>
#include <cstring>
#include "cpp_function.hpp"

extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        auto start_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        string cur_value_str = string{arg_values[0]};
        auto obj = library->create_object(cur_value_str.length());
        auto val = static_cast<char *>(obj->get_value());
        memcpy(val, cur_value_str.c_str(), cur_value_str.size());
        library->send_object(obj, true);

        return 0;
    }
}