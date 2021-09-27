from bench_common import *
from mr_framework import MR
import subprocess

func_num = 200
map_inputs = [f'sort_map{i}' for i in range(func_num)]
number_of_records = 500000

# generate input data
for i, key_name in enumerate(map_inputs):
    begin = i * number_of_records
    data = subprocess.check_output(["/tmp/gensort",
                "-b" + str(begin),
                str(number_of_records),
                "/dev/stdout"])
    client.put_kvs_object(key_name, data)

print(f'Uploaded {len(map_inputs)} map inputs, each with {number_of_records} objects')

mr = MR(client)
app = mr.map_reduce('sort_map.cpp', 'sort_reduce.cpp', map_inputs, kvs_input=True, custom_key='sort_utils.cpp', partition_function='sort_partition.cpp')
mr.compute(app)
