#include <iostream>
#include <chrono>
#include <cstring>
#include "cpp_function.hpp"

extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        auto func_start_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (arg_size < 1){
            std::cout << "Write function args size " << arg_size << " < 1" << std::endl;
            return 1;
        }
        std::string size_str(arg_values[0]);
        int size = stoi(size_str);

        auto ephe_obj = library->create_object(size);
        auto val = static_cast<char *>(ephe_obj->get_value());
        // write data
        memset(val, '1', size - 1);
        val[size - 1] = '\0';

        auto gen_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        library->send_object(ephe_obj);

        auto sent_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        std::cout << "Write function data size " << size << ", start: " << func_start_t << ", gen: " << gen_t << ", sent: " << sent_t << std::endl;
        return 0;
    }
}