void reduce_function(string key, vector<int> &values) {
    int sum = 0;
    for (auto& v : values) sum += v;
    emit(key, sum);
}