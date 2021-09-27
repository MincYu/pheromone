#include <iostream>
#include <chrono>
#include <cstring>
#include <vector>
#include <unordered_map>
#include "cpp_function.hpp"
using string = std::string;


extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        auto func_start_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (arg_size < 1){
            std::cout << "Preprocessing function args size " << arg_size << " < 1" << std::endl;
            return 1;
        }
        char * key_start_index = arg_values[0];
        char * val_start_index = arg_values[0];
        char * cur_pos;
        bool is_ad_id_field, is_event_type_field, is_concerned_event = false;
        std::vector<string> concerned_ad_ids;

        string ad_id;
        for (cur_pos = arg_values[0]; *cur_pos != '\0'; cur_pos++) {
            if (*cur_pos == ':'){
                string key_str{key_start_index, cur_pos};
                val_start_index = cur_pos + 1;
                is_event_type_field = key_str == "event_type";
                is_ad_id_field = key_str == "ad_id";
            }
            else if (*cur_pos == ','){
                if (is_event_type_field) {
                    string event{val_start_index, cur_pos};
                    if (event == "purchase") {
                        is_concerned_event = true;
                    }
                }
                if (is_ad_id_field) {
                    ad_id = string{val_start_index, cur_pos};
                }
                key_start_index = cur_pos + 1;
            }
            else if (*cur_pos == ';'){
                if (is_concerned_event) concerned_ad_ids.push_back(ad_id);
                is_ad_id_field, is_event_type_field, is_concerned_event = false;
                key_start_index = cur_pos + 1;
            }
        }

        if (concerned_ad_ids.size() > 0 ){
            auto ephe_obj = library->create_object(36 * concerned_ad_ids.size());
            auto val = static_cast<char*>(ephe_obj->get_value());
            int cur_pos_idx = 0;

            for (auto &ad_id : concerned_ad_ids){
                // std::cout << "Concerned ad id : " << ad_id << std::endl;
                memcpy(val + cur_pos_idx, ad_id.c_str(), 36);
                cur_pos_idx += 36;
            }
            library->send_object(ephe_obj);
        }

        auto func_end_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        std::cout << "Preprocess function concerned count " << concerned_ad_ids.size() << ", start: " << func_start_t << ", end: " << func_end_t << std::endl;

        return 0;
    }
}