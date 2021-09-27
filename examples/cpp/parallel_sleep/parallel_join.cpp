#include <iostream>
#include <chrono>
#include <cstring>
#include "cpp_function.hpp"

extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        auto start_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::cout << "Parallel join function start: " << start_t << ", size: " << arg_size << std::endl;

        auto cur_value_str = std::to_string(arg_size);

        auto obj = library->create_object(arg_size * 14);
        auto val = static_cast<char *>(obj->get_value());

        size_t cur_pos = 0;
        for (int i = 0; i < arg_size; i++){
            memcpy(val + cur_pos, arg_values[i], 13);
            val[cur_pos + 13] = ',';
            cur_pos += 14;
        }

        library->send_object(obj, true);
        return 0;
    }
}