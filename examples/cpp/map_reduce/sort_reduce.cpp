// void reduce_function(int key, vector<string> values) {
//     emit(key, "");
// }
void reduce_function(char* key, vector<string> values) {
    emit(key, empty_string);
}