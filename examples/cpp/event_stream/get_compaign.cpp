#include <iostream>
#include <chrono>
#include <cstring>
#include <vector>
#include <unordered_map>
#include "cpp_function.hpp"
using string = std::string;

std::unordered_map<string, int> ad_company_map = {
    {"5ef9c3e4-5245-4acd-8e2d-2df0fb986046", 0},
    {"b29468ff-15cc-4701-84ce-b0ac3a7eace9", 1},
    {"6e553901-eea5-4cc1-874b-7e7c72a2e6a9", 2},
    {"eb3c6b5a-567b-42a9-9ba1-c1c43652ab23", 3},
    {"1b8b9181-a1b5-46df-9142-0dc93d6bc608", 4},
    {"205a944b-1a6b-4ee5-9528-70f421e1d3f9", 5},
    {"217e0d9b-247b-49a4-9301-38f51a2bd25a", 6},
    {"11764745-cc0b-4c2a-95d3-d6e756070d06", 7},
    {"5f354793-8726-4d95-9acb-374983213d54", 8},
    {"179a4bd3-e7d7-47a6-9ffc-f8176005b33d", 9}
};

     
extern "C" {
    int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
        auto func_start_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (arg_size < 1){
            std::cout << "Company_id function args size " << arg_size << " < 1" << std::endl;
            return 1;
        }
        std::unordered_map<int, int> comp_id_counts;
        char * cur_pos;

        for (cur_pos = arg_values[0]; *cur_pos != '\0'; cur_pos+=36) {
            string ad_id{cur_pos, 36};
            if (ad_company_map.find(ad_id) != ad_company_map.end()){
                comp_id_counts[ad_company_map[ad_id]]++;
            }
            else {
                std::cout << "Ad id " << ad_id << " not found" << std::endl;
            }
        }

        auto ephe_obj = library->create_object("count", false, 100);
        auto val = static_cast<char*>(ephe_obj->get_value());
        int cur_pos_idx = 0;
        for (auto pair: comp_id_counts){
            string tmp_str = std::to_string(pair.first);
            auto comp_size = tmp_str.size();
            memcpy(val + cur_pos_idx, tmp_str.c_str(), comp_size);
            val[cur_pos_idx + comp_size] = ':';
            
            tmp_str = std::to_string(pair.second);
            auto val_size = tmp_str.size();
            memcpy(val + cur_pos_idx + comp_size + 1, tmp_str.c_str(), val_size);
            val[cur_pos_idx + comp_size + val_size + 1] = ',';
            cur_pos_idx += comp_size + val_size + 2;
        }
        ephe_obj->update_size(cur_pos_idx);
        library->send_object(ephe_obj);

        auto func_end_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        std::cout << "Company_id function start: " << func_start_t << ", end: " << func_end_t << std::endl;

        return 0;
    }
}