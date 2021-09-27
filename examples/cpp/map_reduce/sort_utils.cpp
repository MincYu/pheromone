using key_type = char*;
using value_type = string;

string empty_string = "";

struct custom_comp {
    bool operator()(char* key_1, char* key_2) {
        // compare the first 10 bytes
        for (int i = 0; i < 10; i++) {
            if(key_1[i] == key_2[i]) continue;
            return key_1[i] < key_2[i];
        }
        return false;
    }
};

inline int dump_key_val_entry(char* dump_dest, std::pair<key_type, value_type> entry){
    memcpy(dump_dest, entry.first, 100);
    return 100;
}

inline int load_key_val(char* load_src, 
            std::map<key_type, vector<value_type>, custom_comp> &reduce_input_map){
    char* cur_pos = load_src;
    reduce_input_map[cur_pos] = {};
    return 100;
}