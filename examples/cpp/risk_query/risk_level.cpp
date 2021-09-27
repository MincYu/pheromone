#include <iostream>
#include <chrono>
#include <cstring>
#include "cpp_function.hpp"

extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        auto func_start_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        string count_str{arg_values[0]};
        int count = stoi(count_str);

        string risk_level = "Low";
        if (count > 20) {
            risk_level = "High";
        }
        else if (count > 0) {
            risk_level = "Medium";
        }

        auto obj = library->create_object(count_str.size() + risk_level.size() + 1);
        auto val = static_cast<char *>(obj->get_value());
        memcpy(val, count_str.c_str(), count_str.size());
        val[count_str.size()] = ',';
        memcpy(val + count_str.size() + 1, risk_level.c_str(), risk_level.size());
        library->send_object(obj, true);

        auto func_end_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::cout << "Level function start: " << func_start_t << ", end: " << func_end_t << std::endl;
        return 0;
    }
}