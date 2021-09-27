from bench_common import *

app_name = 'io-test'
data_size = 1000

def register_one_to_many(read_num):
    read_funcs = [f'read_func{i + 1}' for i in range(read_num)]
    dependency = (['write_func1'], read_funcs, DIRECT)
    client.register_app(app_name, ['write_func1'] + read_funcs, [dependency])


def call_one_to_many(times, size):
    for i in range(times):
        client.call_app(app_name, [('write_func1', [size])])
        time.sleep(0.5)

register_one_to_many(1)
time.sleep(0.5)
call_one_to_many(12, 1)

def register_many_to_one(write_num):
    client.register_app(app_name, ['read_func1'] + [f'write_func{i + 1}' for i in range(write_num)], [])
    client.create_bucket(app_name, 'b_read_func')
    client.add_trigger(app_name, 'b_read_func', 't_read_func', BY_BATCH_SIZE, {'function': 'read_func1', 'count': write_num})


def call_many_to_one(times, size, write_num):
    for i in range(times):
        client.call_app(app_name, [(f'write_func{i + 1}', [size]) for i in range(write_num)])
        time.sleep(0.5)

# num = 16
# register_many_to_one(num)
# time.sleep(0.5)
# call_many_to_one(12, 1, num)