#include <iostream>
#include <chrono>
#include <cstring>
#include "cpp_function.hpp"

extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        auto func_start_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (arg_size != 1){
            std::cout << "Read function args size " << arg_size << " != 1" << std::endl;
            return 1;
        }

        char *data_ptr = (arg_values[0]);

        std::string data_size = std::to_string(std::strlen(data_ptr));
        auto ephe_obj = library->create_object(data_size.length());
        auto val = static_cast<char *>(ephe_obj->get_value());
        memcpy(val, data_size.c_str(), data_size.length());
        library->send_object(ephe_obj, true);

        std::cout << "Read function success. start: " << func_start_t << ", size: " << std::strlen(data_ptr) << std::endl;
        return 0;
    }
}