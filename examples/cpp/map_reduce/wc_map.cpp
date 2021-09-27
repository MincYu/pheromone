void map_function(const char* input, size_t data_size) {
    string input_str{input, data_size};
    vector<string> words;
    split_string(input_str, ' ', words);
    for (auto &w : words) emit(w, 1);
}