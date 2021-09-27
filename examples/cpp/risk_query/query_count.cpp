#include <iostream>
#include <chrono>
#include <cstring>
#include <vector>
#include <unordered_map>
#include "cpp_function.hpp"
using string = std::string;

std::unordered_map<string, int> loc_with_confirmed_cases = {
    {"University A", 2},
    {"Building B", 10},
    {"High School C", 4},
    {"Mall D", 21},
    {"Sport Center E", 32},
    {"Park F", 5},
    {"Apartment G", 13}
};


extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        auto func_start_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        string loc{arg_values[0]};
        int confirmed_cases = 0;
        if (loc_with_confirmed_cases.find(loc) != loc_with_confirmed_cases.end()){
            confirmed_cases = loc_with_confirmed_cases[loc];
        }
        string count_str = std::to_string(confirmed_cases);
        auto obj = library->create_object(count_str.size());
        auto val = static_cast<char *>(obj->get_value());
        memcpy(val, count_str.c_str(), count_str.size());
        library->send_object(obj);

        auto func_end_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::cout << "Query function start: " << func_start_t << ", end: " << func_end_t << std::endl;
        return 0;
    }
}