from bench_common import *

app_name = 'ml-pipeline'


# redundant requests
n = 4
serving_funcs = [f'label_image{i + 1}' for i in range(n)]
dependency1 = (['resize'], serving_funcs, DIRECT)
dependency2 = (serving_funcs, ['average'], K_OUT_OF_N, {'k': 3, 'n': n})
client.register_app(app_name, ['resize', 'average'] + serving_funcs, [dependency1, dependency2])
time.sleep(0.5)
client.call_app(app_name, [('resize', [0])], synchronous=True)
num_request = 100
all_times = []
for i in range(num_request):
    start_t = time.time()
    res = client.call_app(app_name, [('resize', [0])], synchronous=True)
    end_t = time.time()
    time.sleep(0.1)
    all_times.append(end_t - start_t)

print(f'elasped: {all_times}')
