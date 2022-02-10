from bench_common import *

app_name = 'parallel-sleep'
parallel_count = 4000

serving_funcs = ['sleep', 'parallel_join']
client.register_app(app_name, serving_funcs, [])
client.create_bucket(app_name, 'b_parallel_join')
client.add_trigger(app_name, 'b_parallel_join', 't_parallel_join', BY_BATCH_SIZE, {'function': 'parallel_join', 'count': parallel_count})

res = client.call_app(app_name, [('sleep', []) for _ in range(parallel_count)], synchronous=True)
# print(res)

num_request = 1
all_times = []
for i in range(num_request):
    start_t = time.time()
    res = client.call_app(app_name, [('sleep', []) for _ in range(parallel_count)], synchronous=True)
    end_t = time.time()
    print(res, end_t - start_t)
    all_times.append(end_t - start_t)
    time.sleep(0.2)
print(f'elasped: {all_times}')
client.delete_bucket(app_name, 'b_parallel_join')


