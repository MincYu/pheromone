from bench_common import *

app_name = 'func-chain'

dependency = (['inc'], ['inc'], DIRECT)
client.register_app(app_name, ['inc'], [dependency])

client.call_app(app_name, [('inc', [0])], synchronous=True)

num_request = 1
all_times = []
for i in range(num_request):
    start_t = time.time()
    res = client.call_app(app_name, [('inc', [0])], synchronous=True)
    end_t = time.time()
    time.sleep(0.1)
    all_times.append(end_t - start_t)

