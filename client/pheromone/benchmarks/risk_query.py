from bench_common import *
import random

app_name = 'risk-query'

dependency_1 = (['filter_loc'], ['query_count'], DIRECT)
dependency_2 = (['query_count'], ['risk_level'], DIRECT)
client.register_app(app_name, ['filter_loc', 'query_count', 'risk_level'], [dependency_1, dependency_2])


loc_list = ["University A", "Building B", "High School C", "Mall D", "Sport Center E", "Park F", "Apartment G"] + [ f'Other Location {i}' for i in range(10)]
def get_input():
    return [f'loc:{random.choice(loc_list)}', f'time:{time.time()}', 'user_id:001']

res = client.call_app(app_name, [('filter_loc', get_input())], synchronous=True)
print(res)

num_request = 12
all_times = []
for i in range(num_request):
    start_t = time.time()
    res = client.call_app(app_name, [('filter_loc', get_input())], synchronous=True)
    print(res)
    end_t = time.time()
    time.sleep(0.1)
    all_times.append(end_t - start_t)
print(all_times)

