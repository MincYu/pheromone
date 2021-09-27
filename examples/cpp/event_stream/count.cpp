#include <iostream>
#include <chrono>
#include <cstring>
#include "cpp_function.hpp"
#include <unordered_map>
#include <sstream>
#include <vector>
using string = std::string;

extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        auto func_start_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::cout << "Count function start: " << func_start_t << ", arg_size: " << arg_size << ", ";

        std::unordered_map<int, int> comp_id_counts;
        int data_size = 0;

        for (int i = 0; i < arg_size; i++){
            char * key_start_index = arg_values[i];
            char * val_start_index = arg_values[i];
            char * cur_pos;
            
            string key_str;
            for (cur_pos = arg_values[i]; *cur_pos != '\0'; cur_pos++) {
                if (*cur_pos == ':'){
                    key_str = string{key_start_index, cur_pos};
                    val_start_index = cur_pos + 1;
                }
                else if (*cur_pos == ','){
                    string count_str{val_start_index, cur_pos};
                    if (!key_str.empty()){
                        auto comp_id = stoi(key_str);
                        comp_id_counts[comp_id] += stoi(count_str);
                    }
                    key_start_index = cur_pos + 1;
                    key_str = string();
                }
                data_size++;
            }

        }
        for (auto pair : comp_id_counts){
            std::cout << pair.first << ": " << pair.second << ", ";
        }

        auto func_end_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        std::cout << "data_size: " << data_size << ", end: " << func_end_t << std::endl;

        return 0;
    }
}