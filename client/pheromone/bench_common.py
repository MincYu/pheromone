import sys
from client import PheromoneClient
from proto.common_pb2 import *
from proto.operation_pb2 import *
import time
import os

client = PheromoneClient('a4c43ae7280b449b38cefc8b0c3d0e38-1320343933.us-east-1.elb.amazonaws.com', 'a025cfc928a6c48f8b4b51e31f1cc278-450220547.us-east-1.elb.amazonaws.com', '3.93.178.173', thread_id=0)
