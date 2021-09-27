#define KEY_TYPE_ID $KEY_TYPE_ID
#define VALUE_TYPE_ID $VALUE_TYPE_ID
#define CUSTOM_KEY $CUSTOM_KEY
#include <iostream>
#include <chrono>
#include <sstream>
#include <map>
#include <vector>
#include "cpp_function.hpp"
#include <cstring>

template <class T>
using vector = std::vector<T>;

#if CUSTOM_KEY
$CUSTOM_UTIL
std::map<key_type, vector<value_type>, custom_comp> reduce_input;

#else
using key_type = $KEY_TYPE;
using value_type = $VALUE_TYPE;

inline string dump_key(key_type key){
#if KEY_TYPE_ID == 0
    return key;
#else
    return std::to_string(key);
#endif
}

inline string dump_value(value_type value){
#if VALUE_TYPE_ID == 0
    return value;
#else
    return std::to_string(value);
#endif
}

inline key_type parse_key(string str){
#if KEY_TYPE_ID == 0
    return str;
#elif KEY_TYPE_ID == 1
    return stoi(str);
#elif KEY_TYPE_ID == 2
    return stol(str);
#elif KEY_TYPE_ID == 3
    return stod(str);
#endif
}

inline value_type parse_value(string str) {
#if VALUE_TYPE_ID == 0
    return str;
#elif VALUE_TYPE_ID == 1
    return stoi(str);
#elif VALUE_TYPE_ID == 2
    return stol(str);
#elif VALUE_TYPE_ID == 3
    return stod(str);
#endif
}

std::map<key_type, vector<value_type>> reduce_input;
#endif

std::pair<key_type, value_type> reduce_output_per_key;

inline void emit(key_type key, value_type value) {
    reduce_output_per_key = std::make_pair(key, value);
}

$PH_REDUCE


inline void split_string(const string& s, char delim, vector<string>& elems) {
  std::stringstream ss(s);
  string item;

  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
}

extern "C" int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
    auto func_start_t = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (arg_size % 2 != 0){
        std::cout << "Reduce function has no valid args " << arg_size << std::endl;
        return 1;
    }

    string key_name{arg_values[0]};
    string group_name = key_name.substr(0, key_name.find(":"));
    int data_size = 0;
    std::cout << "Reduce group id: " << group_name << ", arg_size: " << arg_size << ". start: " << func_start_t;

    for (int i = 1; i < arg_size; i+=2) {

#if CUSTOM_KEY
        char * cur_pos = arg_values[i];
        size_t input_arg_offset = 0;
        size_t arg_size_in_bytes = library->get_size_of_arg(i);
        while (input_arg_offset < arg_size_in_bytes) {
            int offset = load_key_val(cur_pos, reduce_input);
            cur_pos += offset;
            input_arg_offset += offset;
        }
        // std::cout << "Arg " << i << " size " << arg_size_in_bytes << " parsed offset " << input_arg_offset << std::endl;
        data_size += arg_size_in_bytes;
#else
        char * key_start_index = arg_values[i];
        char * val_start_index = arg_values[i];
        char * cur_pos;
        
        string key_str;
        for (cur_pos = arg_values[i]; *cur_pos != '\0'; cur_pos++) {
            if (*cur_pos == ','){
                key_str = string{key_start_index, cur_pos};
                val_start_index = cur_pos + 1;
            }
            else if (*cur_pos == ';'){
                string val_str{val_start_index, cur_pos};
                if (!key_str.empty()){
                    if (val_str.empty()) {
                        reduce_input[parse_key(key_str)] = {};
                    }
                    else{
                        reduce_input[parse_key(key_str)].push_back(parse_value(val_str));
                    }
                }
                key_start_index = cur_pos + 1;
                key_str = string();
            }
            data_size++;
        }
#endif

    }

    auto parse_input_t = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::cout << ", parse: " << parse_input_t;
    auto obj = library->create_object("kvs", "reduce" + group_name, 50 * 1024 * 1024);
    auto val = static_cast<char*>(obj->get_value());
    size_t cur_offset = 0;

    for (auto &pair : reduce_input) {
        reduce_function(pair.first, pair.second);
#if CUSTOM_KEY
        int offset = dump_key_val_entry(val + cur_offset, reduce_output_per_key);
        cur_offset += offset;
#else
        string key_str = dump_key(reduce_output_per_key.first);
        auto key_size = key_str.size();
        memcpy(val + cur_offset, key_str.c_str(), key_size);
        cur_offset += key_size;

        string tmp_str = dump_value(reduce_output_per_key.second);
        auto val_size = tmp_str.size();
        memcpy(val + cur_offset, tmp_str.c_str(), val_size);
        cur_offset += val_size;
#endif
    }

    auto reduce_t = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::cout << ", reduce: " << reduce_t;
    obj->update_size(cur_offset);
    library->send_object(obj, true);

    reduce_input.clear();
    auto end_t = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::cout  << ", end: " << end_t  << ", data_size: " << data_size << ", out_size: " << cur_offset << std::endl;
    return 0;
}