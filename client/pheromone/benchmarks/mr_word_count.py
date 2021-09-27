from bench_common import *
from mr_framework import MR

func_num = 12
map_inputs = [f'map{i}' for i in range(func_num)]

# repeat_times = int(400 / func_num)
text_path = '/home/ubuntu/wc.txt'
with open(text_path, 'r') as f:
    text_data = f.read().replace('\n', ' ') * 2

    for key in map_inputs:
        client.put_kvs_object(key, text_data)

print(f'Uploaded {len(map_inputs)} map inputs')

mr = MR(client)
app = mr.map_reduce('wc_map.cpp', 'wc_reduce.cpp', map_inputs, kvs_input=True, reduce_num=6, partition_function=None)
mr.compute(app)
