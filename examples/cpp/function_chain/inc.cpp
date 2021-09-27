#include <iostream>
#include <chrono>
#include <cstring>
#include "cpp_function.hpp"

extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        auto start_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        // std::cout << "sleep function end: " << end_t << std::endl;
        int cur_value = stoi(string{arg_values[0]}) + 1;
        auto obj = library->create_object(5);
        auto val = static_cast<char *>(obj->get_value());

        auto cur_value_str = std::to_string(cur_value);
        memcpy(val, cur_value_str.c_str(), cur_value_str.size());
        val[cur_value_str.size()] = '\0';
        if (cur_value >= 500){
            library->send_object(obj, true);
        }
        else {
            library->send_object(obj);
        }
        auto end_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();   
        std::cout << "Inc function value: " << cur_value << ", start: " << start_t << ", end: " << end_t << std::endl;
        return 0;
    }
}