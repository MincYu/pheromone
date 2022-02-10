import os
import subprocess
import sys
import shutil
import subprocess
import json
from proto.common_pb2 import *
from proto.operation_pb2 import *
from client import PheromoneClient
from functools import reduce
import argparse

type_id_map = {'string': '0', 'int': '1', 'long': '2', 'double': '3'}

class MR:
    def __init__(self, client, path=os.path.join(os.getcwd(), "../../examples/cpp/map_reduce")):
        self.workspace = path
        self.client = client
        self.app_inputs = {}
        self.kvs_input = False
    
    def map_reduce(self, map_function, reduce_function, map_inputs, kvs_input=False, reduce_num=2, custom_key=None, partition_function=None):
        # create actual map and reduce functions
        map_func_str = self._read_file_as_str(os.path.join(self.workspace, map_function))
        reduce_func_str = self._read_file_as_str(os.path.join(self.workspace, reduce_function))
        # a hack to get the key/value data type
        custom_utils = ""
        key_type, value_type = "string", "string"
        if custom_key:
            custom_utils = self._read_file_as_str(os.path.join(self.workspace, custom_key))
        else:
            key_type_start = reduce_func_str.find('(')
            key_type_end = reduce_func_str.find(' ', key_type_start)
            key_type = reduce_func_str[key_type_start+1:key_type_end]
            if key_type not in type_id_map:
                print(f'key type {key_type} is not supported')
                return 

            value_type_start = reduce_func_str.find('<')
            value_type_end = reduce_func_str.find('>', value_type_start)
            value_type = reduce_func_str[value_type_start+1:value_type_end]
            if value_type not in type_id_map:
                print(f'value type {value_type} is not supported')
                return 

        partition_func_str = ""
        if partition_function:
            partition_func_str = self._read_file_as_str(os.path.join(self.workspace, partition_function))

        map_vars = {
            'PARTITION': str(1) if partition_function else str(0),
            'KEY_TYPE_ID': type_id_map[key_type],
            'VALUE_TYPE_ID': type_id_map[value_type],
            'KEY_TYPE': key_type,
            'VALUE_TYPE': value_type,
            'CUSTOM_KEY': '1' if custom_key else '0',
            'CUSTOM_UTIL': custom_utils,
            'REDUCE_NUM': str(reduce_num),
            'PH_MAP': map_func_str,
            'PH_REDUCE': reduce_func_str,
            'PH_PARTITION': partition_func_str,
        }
        self._change_variables_in_file(os.path.join(self.workspace, 'mapper_template.cpp'), os.path.join(self.workspace, 'mapper.cpp'), map_vars)

        reduce_vars = {
            'KEY_TYPE_ID': type_id_map[key_type],
            'VALUE_TYPE_ID': type_id_map[value_type],
            'KEY_TYPE': key_type,
            'CUSTOM_KEY': '1' if custom_key else '0',
            'CUSTOM_UTIL': custom_utils,
            'VALUE_TYPE': value_type,
            'PH_REDUCE': reduce_func_str
        }
        self._change_variables_in_file(os.path.join(self.workspace, 'reducer_template.cpp'), os.path.join(self.workspace, 'reducer.cpp'), reduce_vars)

        # build .so
        map_func_name = map_function.split('.')[0]
        reduce_func_name = reduce_function.split('.')[0]
        print('Compiling the mapper and reducer...')
        command = subprocess.run([f'cd {self.workspace} && bash compile_upload.sh {map_func_name} {reduce_func_name}'], shell=True)

        # deploy app
        app_name = map_func_name + '_' + reduce_func_name
        self.client.register_app(app_name, [map_func_name, reduce_func_name], [])
        bucket_name = f'b_{app_name}'
        res = self.client.create_bucket(app_name, bucket_name)
        if not res:
            self.client.delete_bucket(app_name, bucket_name)
            res = self.client.create_bucket(app_name, bucket_name)
        
        if res:
            res = self.client.add_trigger(app_name, bucket_name, f't_{app_name}', DYNAMIC_GROUP, {'function': reduce_func_name, 'control_group': [ f'ctrl:{i}' for i in range(len(map_inputs))]}, trigger_option=1)
            self.app_inputs[app_name] = (map_func_name, map_inputs)
            self.kvs_input = kvs_input
            return app_name
        else:
            print(f'Error {res} in deployment')

    def compute(self, app):
        if self.kvs_input:
            args = [(self.app_inputs[app][0], [f'b_{app}', i, d, '1']) for i, d in enumerate(self.app_inputs[app][1])]
        else:
            args = [(self.app_inputs[app][0], [f'b_{app}', i, d]) for i, d in enumerate(self.app_inputs[app][1])]
        self.client.call_app(app, args)

    def _read_file_as_str(self, file_path):
        res = ""
        with open(file_path, 'r') as f:
            for l in f:
                res += l
        return res

    def _change_variables_in_file(self, tem_path, file_path, var_values):
        data = []
        with open(tem_path, 'r') as f:
            for l in f:
                for var, val in var_values.items():
                    if f'${var}' in l:
                        l = l.replace(f'${var}', val)
                data.append(l)

        with open(file_path, 'w') as f:
            f.writelines(data)