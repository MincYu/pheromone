int partition_num = 200;
uint32_t range_per_par = UINT32_MAX / partition_num;

int partition(char* key) {
    uint32_t par_indicate = reinterpret_cast<uint32_t *>(key)[0];
    int partition_idx = par_indicate / range_per_par;
    return partition_idx < partition_num ? partition_idx : partition_num - 1;
}