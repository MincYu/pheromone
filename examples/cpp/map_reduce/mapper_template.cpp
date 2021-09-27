#define PARTITION $PARTITION
#define KEY_TYPE_ID $KEY_TYPE_ID
#define VALUE_TYPE_ID $VALUE_TYPE_ID
#define CUSTOM_KEY $CUSTOM_KEY
#define REDUCE_NUM $REDUCE_NUM
#include <iostream>
#include <stdint.h>
#include <sstream>
#include <chrono>
#include <cstring>
#include <vector>
#include <map>
#include <unordered_map>
#include "cpp_function.hpp"

template <class T>
using vector = std::vector<T>;

#if CUSTOM_KEY
$CUSTOM_UTIL
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

#endif

std::unordered_map<key_type, vector<value_type>> key_values;
std::unordered_map<int, std::pair<EpheObject*, size_t>> ephe_objs;

std::pair<key_type, value_type> combined_key_value;

int cur_phase = 0; // map (0), combine (1)

inline void emit(key_type key, value_type value) {
    if (cur_phase == 0) {
        key_values[key].push_back(value);
    }
    else if (cur_phase == 1){
        combined_key_value = std::make_pair(key, value);
    }
}


inline void split_string(const string& s, char delim, vector<string>& elems) {
  std::stringstream ss(s);
  string item;

  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
} 

$PH_MAP

$PH_REDUCE

#if PARTITION
$PH_PARTITION
#else
std::hash<key_type> h;
int partition(key_type key) {
    return h(key) % REDUCE_NUM;
}
#endif

extern "C" int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
    auto func_start_t = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (arg_size < 3){
        std::cout << "Map function has no valid args" << std::endl;
        return 1;
    }
    

    // the first arg is the bucket name
    string group_bucket(arg_values[0]);

    // the index of input partition
    string index_str(arg_values[1]);

    // this arg should be the input to the map function
    
    // string input(arg_values[2]);
    // int input_size = input.size();
    
    char* input = arg_values[2];
    int input_size = strlen(input);

    // from durable kvs
    if (arg_size > 3 && string(arg_values[3]) == "1"){
        auto data = library->get_object("", string{input}, false);
        input_size = data->get_size();
        input = static_cast<char*>(data->get_value());
    }

    auto parse_input_t = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::cout << "Map function id: " << index_str << ", input size: " << input_size << ", start: " << func_start_t  << ", parse: " << parse_input_t;

    cur_phase = 0;
    map_function(input, input_size);

    auto map_t = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::cout << ", map: " << map_t;
    cur_phase = 1;
    for (auto &pair : key_values) {
        reduce_function(pair.first, pair.second);
        int reduce_id = partition(combined_key_value.first);
        if (ephe_objs.find(reduce_id) == ephe_objs.end()){
            ephe_objs[reduce_id].first = library->create_object(group_bucket, std::to_string(reduce_id) + ":" + library->gen_unique_key(), 4 * 1024 * 1024);
            ephe_objs[reduce_id].second = 0;
        }
        auto cur_pos = static_cast<char*>(ephe_objs[reduce_id].first->get_value()) + ephe_objs[reduce_id].second;

#if CUSTOM_KEY

        int offset = dump_key_val_entry(cur_pos, combined_key_value);
        ephe_objs[reduce_id].second += offset;

#else

        string key_str = dump_key(combined_key_value.first);
        auto key_size = key_str.size();
        memcpy(cur_pos, key_str.c_str(), key_size);

        cur_pos[key_size] = ','; // splitter key/values
        string tmp_str = dump_value(combined_key_value.second);
        auto val_size = tmp_str.size();
        memcpy(cur_pos + key_size + 1, tmp_str.c_str(), val_size);
        cur_pos[key_size + val_size + 1] = ';'; // splitter records
        ephe_objs[reduce_id].second += key_size + val_size + 2;

#endif
        // // consider using fix-sized data
        // cur_pos[key_size + 4] = (combined_v >> 24) & 0xFF;
        // cur_pos[key_size + 3] = (combined_v >> 16) & 0xFF;
        // cur_pos[key_size + 2] = (combined_v >> 8) & 0xFF;
        // cur_pos[key_size + 1] = combined_v & 0xFF;
        // ephe_objs[reduce_id].second += key_size + 5;
    }

    int output_num = 0;
    for (auto &pair : ephe_objs) {
        pair.second.first->update_size(pair.second.second);
        library->send_object(pair.second.first);
        output_num++;
    }

    auto collect_t = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::cout << ", collect: " << collect_t;

    // TODO more straightforward when sending signal data, e.g., send_batch_object
    auto ctrl_obj = library->create_object(group_bucket, "ctrl:" + index_str, 1);
    static_cast<char*>(ctrl_obj->get_value())[0] = '1';
    library->send_object(ctrl_obj);

    auto last_t = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::cout << ", last: " << last_t;

    key_values.clear();
    ephe_objs.clear();
    cur_phase = 0;
    
    auto end_t = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::cout << ", end: " << end_t << ", output num: " << output_num << std::endl;

    return 0;
}

