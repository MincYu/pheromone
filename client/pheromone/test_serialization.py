import sys
from client import PheromoneClient
from proto.common_pb2 import *
from proto.operation_pb2 import *
import time
import os

# client = PheromoneClient('a1f7912a3bf404bbcba15e1367345190-1317238364.us-east-1.elb.amazonaws.com', 'ad0dc9175284b4cdf80bf7c8ed5ffa8c-352615241.us-east-1.elb.amazonaws.com', '18.212.247.15', thread_id=0)
client = PheromoneClient('aa94cebdc78bd46108b36d43ec7b49ec-1226275909.us-east-1.elb.amazonaws.com', 'aa9317d364d9848e5b0ece8d9e4d5892-2061968069.us-east-1.elb.amazonaws.com', '54.88.41.113', thread_id=0)

app_name = 'func-chain'
# client.put_kvs_object("aaa","bbb")
# res = client.get_kvs_object("k_anna_write8")

dependency = (['write_func'], ['read_func'], DIRECT)
client.register_app(app_name, ['write_func', 'read_func'], [dependency])

res = client.call_app(app_name, [('write_func', [100000000])], synchronous=True)

print(res)

