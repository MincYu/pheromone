void map_function(const char* input, size_t data_size){
    char* cur_pos = const_cast<char*>(input);
    for (int i = 0; i < data_size; i+=100) emit(cur_pos + i, empty_string); // 100 bytes per record
}