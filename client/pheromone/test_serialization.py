import sys
from client import PheromoneClient
from proto.common_pb2 import *
from proto.operation_pb2 import *
import time
import os

# client = PheromoneClient('a1f7912a3bf404bbcba15e1367345190-1317238364.us-east-1.elb.amazonaws.com', 'ad0dc9175284b4cdf80bf7c8ed5ffa8c-352615241.us-east-1.elb.amazonaws.com', '18.212.247.15', thread_id=0)
client = PheromoneClient('127.0.0.1', '127.0.0.1', '127.0.0.1', thread_id=0)

app_name = 'func-chain'

dependency = (['inc'], ['inc'], DIRECT)
client.register_app(app_name, ['inc'], [dependency])

res = client.call_app(app_name, [('inc', [[1,2,3,4,5]])], synchronous=True)

print(res)

