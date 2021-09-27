#include <iostream>
#include <chrono>
#include <cstring>
#include "cpp_function.hpp"

extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        auto func_start_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        string loc;
        for (int i = 0; i < arg_size; i++){
            string key_val = string{arg_values[i]};
            auto key = key_val.substr(0, key_val.find(":"));
            if (key == "loc"){
                loc = key_val.substr(key_val.find(":") + 1);
                break;
            }
        }

        if (!loc.empty()){
            auto obj = library->create_object(loc.size());
            auto val = static_cast<char *>(obj->get_value());
            memcpy(val, loc.c_str(), loc.size());
            library->send_object(obj);
        }
        auto func_end_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::cout << "Filter function start: " << func_start_t << ", end: " << func_end_t << std::endl;
        return 0;
    }
}