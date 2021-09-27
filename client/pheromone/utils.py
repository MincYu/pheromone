
FUNC_CREATE_PORT = 5000
APP_REGIST_PORT = 5020
FUNC_CALL_PORT = 5050
COORD_QUERY_PORT = 6002
BUCKET_OP_PORT = 7800
TRIGGER_OP_PORT = 7900

BUCKET_REQ_PORT = 12070
TRIGGER_REQ_PORT = 12170

"""
Connection thread
"""
class Thread():
    def __init__(self, ip, tid):
        self.ip = ip
        self.tid = tid

        self._base = 'tcp://*:'
        self._ip_base = 'tcp://' + self.ip + ':'

    def get_ip(self):
        return self.ip

    def get_tid(self):
        return self.tid

class OperationRequestThread(Thread):
    def bucket_req_connect_address(self):
        return self._ip_base + str(self.tid + BUCKET_REQ_PORT)

    def bucket_req_bind_address(self):
        return self._base + str(self.tid + BUCKET_REQ_PORT)

    def trigger_req_connect_address(self):
        return self._ip_base + str(self.tid + TRIGGER_REQ_PORT)

    def trigger_req_bind_address(self):
        return self._base + str(self.tid + TRIGGER_REQ_PORT)

class OperationThread(Thread):
    def bucket_op_connect_address(self):
        return self._ip_base + str(self.tid + BUCKET_OP_PORT)

    def trigger_op_connect_address(self):
        return self._ip_base + str(self.tid + TRIGGER_OP_PORT)

    def call_connect_address(self):
        return self._ip_base + str(self.tid + FUNC_CALL_PORT)

    def app_regist_connect_address(self):
        return self._ip_base + str(self.tid + APP_REGIST_PORT)

"""
zmq utils from Cloudburst
"""
def send_request(req_obj, send_sock):
    req_string = req_obj.SerializeToString()

    send_sock.send(req_string)


def recv_response(req_ids, rcv_sock, resp_class):
    responses = []

    while len(responses) < len(req_ids):
        resp_obj = resp_class()
        resp = rcv_sock.recv()
        resp_obj.ParseFromString(resp)

        while resp_obj.response_id not in req_ids:
            resp_obj.Clear()
            resp_obj.ParseFromString(rcv_sock.recv())

        responses.append(resp_obj)

    return responses


class SocketCache():
    def __init__(self, context, zmq_type):
        self.context = context
        self._cache = {}
        self.zmq_type = zmq_type

    def get(self, addr):
        if addr not in self._cache:
            sock = self.context.socket(self.zmq_type)
            sock.connect(addr)

            self._cache[addr] = sock

            return sock
        else:
            return self._cache[addr]
    
    def send(self, addr, req_obj):
        sckt = self.get(addr)
        req_string = req_obj.SerializeToString()
        sckt.send(req_string)
