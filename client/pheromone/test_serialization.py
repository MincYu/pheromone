import sys
from client import PheromoneClient
from proto.common_pb2 import *
from proto.operation_pb2 import *
import time
import os

# client = PheromoneClient('a1f7912a3bf404bbcba15e1367345190-1317238364.us-east-1.elb.amazonaws.com', 'ad0dc9175284b4cdf80bf7c8ed5ffa8c-352615241.us-east-1.elb.amazonaws.com', '18.212.247.15', thread_id=0)
client = PheromoneClient('a46163c73e16b416396314ca485f15d5-1622429709.us-east-1.elb.amazonaws.com', 'a8940347b05664c6ea9294fcf41d6e21-1576752125.us-east-1.elb.amazonaws.com', '54.88.41.113', thread_id=0)

app_name = 'func-chain-new'

dependency = (['first'], ['second'], DIRECT)
client.register_app(app_name, ['first', 'second'], [dependency])
param = []
for i in range(100000):
    param.append(i)
res = client.call_app(app_name, [('first', [param])], synchronous=True)

print(res)

