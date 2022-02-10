import logging
import os
import zmq
import socket
from utils import *
from proto.operation_pb2 import *
from proto.common_pb2 import *
import numpy as np
import time
from anna.client import AnnaTcpClient
from anna.lattices import (
    LWWPairLattice
)

def generate_timestamp(tid=1):
    t = time.time()
    p = 10
    while tid >= p:
        p *= 10
    return int(t * p + tid)

default_rerun_timeout = 3000

class PheromoneClient():
    def __init__(self, mngt_ip, kvs_addr, ip, thread_id=0, context=None):
        if not context:
            self.context = zmq.Context(1)
        else:
            self.context = context
        
        if ip:
            self.ort = OperationRequestThread(ip, thread_id)
        else:  # If the IP is not provided, we attempt to infer it.
            self.ort = OperationRequestThread(socket.gethostbyname(socket.gethostname()), thread_id)

        self.kvs_client = AnnaTcpClient(kvs_addr, ip, local=False, offset=thread_id + 10)

        # currently the coordinator by default runs on thread 0
        self.mngt_socket = self.context.socket(zmq.REQ)
        self.mngt_socket.connect("tcp://" + mngt_ip + ':' + str(COORD_QUERY_PORT))

        self.app_coords = {}
        self.pusher_cache = SocketCache(self.context, zmq.PUSH)

        self.bucket_op_puller = self.context.socket(zmq.PULL)
        self.bucket_op_puller.bind(self.ort.bucket_req_bind_address())

        self.trigger_op_puller = self.context.socket(zmq.PULL)
        self.trigger_op_puller.bind(self.ort.trigger_req_bind_address())

        self.response_puller = self.context.socket(zmq.PULL)
        response_port = 9000 + thread_id
        self.response_puller.setsockopt(zmq.RCVTIMEO, 5000)
        self.response_puller.bind('tcp://*:' + str(response_port))
        self.response_address = 'tcp://' + ip + ':' + str(response_port)

        self.rid = 0
    
    def get_kvs_object(self, key):
        lattice = self.kvs_client.get(key)[key]
        return lattice.reveal()

    def put_kvs_object(self, key, value):
        if isinstance(value, bytes):
            lattice = LWWPairLattice(generate_timestamp(0), value)
        else: # string
            lattice = LWWPairLattice(generate_timestamp(0), bytes(value, 'utf-8'))
        return self.kvs_client.put(key, lattice)

    def create_bucket(self, app_name, bucket_name):
        coord_thread = self._try_get_app_coord(app_name)
        req = BucketOperationRequest()
        req = self._prepare_request(req, CREATE_BUCKET, self.ort.bucket_req_connect_address())
        
        req.bucket_name = bucket_name
        req.app_name = app_name

        self.pusher_cache.send(coord_thread.bucket_op_connect_address(), req)

        response = recv_response([req.request_id], self.bucket_op_puller, BucketOperationResponse)[0]

        if response.error == 0:
            logging.info('Successfully creating bucket {}'.format(bucket_name))
            return True
        else:
            logging.error('Error {} in creating bucket {}'.format(response.error, bucket_name))
            return False

    def delete_bucket(self, app_name, bucket_name):
        coord_thread = self._try_get_app_coord(app_name)
        req = BucketOperationRequest()
        req = self._prepare_request(req, DELETE_BUCKET, self.ort.bucket_req_connect_address())

        req.bucket_name = bucket_name
        req.app_name = app_name

        self.pusher_cache.send(coord_thread.bucket_op_connect_address(), req)
        response = recv_response([req.request_id], self.bucket_op_puller, BucketOperationResponse)[0]

        if response.error == 0:
            logging.info('Successfully deleting bucket {}'.format(bucket_name))
            return True
        else:
            logging.error('Error {} in deleting bucket {}'.format(response.error, bucket_name))
            return False

    def add_trigger(self, app_name, bucket_name, trigger_name, primitive_type, primitive, trigger_option=0, hints=None):
        coord_thread = self._try_get_app_coord(app_name)

        req = TriggerOperationRequest()
        req = self._prepare_request(req, ADD_TRIGGER, self.ort.trigger_req_connect_address())
        
        req.app_name = app_name
        req.bucket_name = bucket_name
        req.trigger_name = trigger_name
        req.primitive_type = primitive_type
        req.trigger_option = trigger_option
        
        if primitive_type == IMMEDIATE:
            prm = ImmediatePrimitive()
            prm.function = primitive['function']
        elif primitive_type == BY_NAME:
            prm = ByNamePrimitive()
            prm.function = primitive['function']
            prm.key_name = primitive['key_name']
        elif primitive_type == BY_BATCH_SIZE:
            prm = ByBatchSizePrimitive()
            prm.function = primitive['function']
            prm.count = primitive['count']
        elif primitive_type == BY_SET:
            prm = BySetPrimitive()
            prm.function = primitive['function']
            for k in primitive['key_set']:
                if not isinstance(k, str):
                    k = str(k)
                prm.key_set.append(k)
        elif primitive_type == DYNAMIC_GROUP:
            prm = DynamicGroupPrimitive()
            prm.function = primitive['function']
            for k in primitive['control_group']:
                prm.control_group.append(k)
        elif primitive_type == REDUNDANT:
            prm = RedundantPrimitive()
            prm.function = primitive['function']
            prm.n = primitive['k']
            prm.k = primitive['n']
        elif primitive_type == BY_TIME:
            prm = ByTimePrimitive()
            prm.function = primitive['function']
            prm.time_window = primitive['time_window']
        
        req.primitive = prm.SerializeToString()

        def parse_hints(req, hint):
            h = req.hints.add()
            h.source_function = hint[0]
            if hint[1] is not None:
                h.source_key = hint[1]
            h.timeout = hint[2] if len(hint) > 2 else default_rerun_timeout

        if hints is not None: 
            if isinstance(hints, tuple):
                parse_hints(req, hints)
            elif isinstance(hints, list):
                for hint in hints:
                    parse_hints(req, hint)
            else:
                logging.error('Unregnized hints {}'.format(hints))
                return False
        send_sock = self.pusher_cache.get(coord_thread.trigger_op_connect_address())

        send_request(req, send_sock)
        response = recv_response([req.request_id], self.trigger_op_puller, TriggerOperationResponse)[0]

        if response.error == 0:
            logging.info('Successfully adding trigger {} to bucket {}'.format(trigger_name, bucket_name))
            return True
        else:
            logging.error('Error {} in adding trigger {} to bucket {}'.format(response.error, trigger_name, bucket_name))
            return False

    def delete_trigger(self, app_name, bucket_name, trigger_name):
        coord_thread = self._try_get_app_coord(app_name)
        req = TriggerOperationRequest()
        req = self._prepare_request(req, DELETE_TRIGGER, self.ort.trigger_req_connect_address())

        req.bucket_name = bucket_name
        req.trigger_name = trigger_name

        send_sock = self.pusher_cache.get(coord_thread.trigger_op_connect_address())

        send_request(req, send_sock)
        response = recv_response([req.request_id], self.trigger_op_puller, TriggerOperationResponse)[0]

        if response.error == 0:
            logging.info('Successfully deleting trigger {} from bucket {}'.format(trigger_name, bucket_name))
            return True
        else:
            logging.error('Error {} in deleting trigger {} from bucket {}'.format(response.error, trigger_name, bucket_name))
            return False

    def create_function(self, name, path):
        pass

    def register_app(self, app_name, functions, dependencies=[]):
        coord_thread = self._try_get_app_coord(app_name)
        msg = AppRegistration()
        msg.app_name = app_name
        for func in functions:
            msg.functions.append(func)
        for d in dependencies:
            dep = msg.dependencies.add()
            for src in d[0]:
                dep.src_functions.append(src)
            for tgt in d[1]:
                dep.tgt_functions.append(tgt)
            dep.type = d[2]
            if len(d) >= 4:
                if d[2] == K_OUT_OF_N:
                    if isinstance(d[3], dict) and 'k' in d[3] and 'n' in d[3]:
                        dep.description = bytes(str(d[3]['k']) + ',' + str(d[3]['n']), 'utf-8')
                    else:
                        logging.error('Error dependency description for K_OUT_OF_N')
                        return
                elif d[2] == PERIODIC:
                    if isinstance(d[3], int):
                        dep.description = bytes(str(d[3]), 'utf-8')
                    else:
                        logging.error('Error dependency description for PERIODIC')
                        return
        self.pusher_cache.send(coord_thread.app_regist_connect_address(), msg)

    def call_app(self, app_name, func_args, synchronous=False, timeout=5000, retry=0):
        self.response_puller.setsockopt(zmq.RCVTIMEO, timeout)
        coord_thread = self._try_get_app_coord(app_name)
        call = FunctionCall()
        # call.name = name
        call.app_name = app_name
        call.request_id = self._get_request_id()
        call.sync_data_status = False
        for func, args in func_args:
            func_req = call.requests.add()
            func_req.name = func
            for arg in args:
                argobj = func_req.arguments.add()
                argobj.body = bytes(str(arg), 'utf-8')
                argobj.arg_flag = 0
        if synchronous:
            call.resp_address = self.response_address

        send_t = time.time()
        self.pusher_cache.send(coord_thread.call_connect_address(), call)

        if synchronous:
            while (True):
                try:
                    r = FunctionCallResponse()
                    r.ParseFromString(self.response_puller.recv())
                    recv_t = time.time()
                    print('Client timer. {}'.format(recv_t - send_t))

                    if r.error_no == 0:
                        return r.output
                    else:
                        logging.error('Error response no. {}'.format(r.error_no))
                        return None
                except zmq.ZMQError as e:
                    if e.errno == zmq.EAGAIN:
                        if retry > 0:
                            logging.error('Request timed out. retrying...')
                            retry -= 1
                            self.pusher_cache.send(coord_thread.call_connect_address(), call)
                        else:
                            logging.error('Request timed out.')
                            return None
                    else:
                        raise e

    def _try_get_app_coord(self, app_name):
        if app_name not in self.app_coords:
            query = CoordQuery()
            query.application = app_name
            query.request_id = self._get_request_id()
            self.mngt_socket.send(query.SerializeToString())

            r = CoordResponse()
            r.ParseFromString(self.mngt_socket.recv())
            self.app_coords[app_name] = OperationThread(r.coord_ip, r.thread_id)
            print(f'Query from management. {r.coord_ip}, {r.thread_id}')
        
        return self.app_coords[app_name]

    def _prepare_request(self, req, op_type, resp_addr):
        req.operation_type = op_type
        req.request_id = str(self._get_request_id())
        req.response_address = resp_addr
        return req

    def _get_request_id(self):
        self.rid += 1
        return str(self.rid)
