

import random
import time
import uuid

import numpy as np
from bench_common import *
from bench_common import DIRECT, PERIODIC, client


compaign_num = 10

ad_types = ['banner', 'modal', 'sponsored-search', 'mail', 'mobile']
event_types = ['view', 'click', 'purchase'] 
ad_company_map = {'5ef9c3e4-5245-4acd-8e2d-2df0fb986046': 0, 'b29468ff-15cc-4701-84ce-b0ac3a7eace9': 1, '6e553901-eea5-4cc1-874b-7e7c72a2e6a9': 2, 'eb3c6b5a-567b-42a9-9ba1-c1c43652ab23': 3, '1b8b9181-a1b5-46df-9142-0dc93d6bc608': 4, '205a944b-1a6b-4ee5-9528-70f421e1d3f9': 5, '217e0d9b-247b-49a4-9301-38f51a2bd25a': 6, '11764745-cc0b-4c2a-95d3-d6e756070d06': 7, '5f354793-8726-4d95-9acb-374983213d54': 8, '179a4bd3-e7d7-47a6-9ffc-f8176005b33d': 9}
ad_id_list = list(ad_company_map.keys())
def gen_ad_event(count):
    events = []
    company_ids = np.random.randint(compaign_num, size=count)

    for c_id in company_ids:
        event_id = ad_id_list[c_id]
        event = f'user_id:{event_id},page_id:{event_id},ad_id:{event_id},ad_type:{random.choice(ad_types)},event_type:{random.choice(event_types)},event_time:{time.time()},ip_address:127.0.0.1'
        events.append(event)
    return ';'.join(events)


app_name = 'event-stream'

dependency_1 = (['preprocess'], ['get_compaign'], DIRECT)
dependency_2 = (['get_compaign'], ['count'], PERIODIC, 1000)
client.register_app(app_name, ['preprocess', 'get_compaign', 'count'], [dependency_1, dependency_2])
print(f'App {app_name} registered')
time.sleep(0.5)

times = 60
total_count = 0
event_cache = [gen_ad_event(10) for i in range(100)]

def send(cl):
    start_t = time.time()
    count = 0
    while True:
        for i in range(0, 10):
            cl.call_app(app_name, [('preprocess', [random.choice(event_cache)])])

        count += 10
        # time.sleep(0.00001)

        if (time.time() - start_t >= times):
            break
    return count

# total_client_num = 1
# clients = [client] + [PheromoneClient('aa17fe6aa76b9414d98925fa4a22f65a-1618286925.us-east-1.elb.amazonaws.com', '107.23.134.206', thread_id=i) for i in range(1, total_client_num)] 
# from concurrent.futures import ThreadPoolExecutor, wait, ALL_COMPLETED
# pool = ThreadPoolExecutor(max_workers=total_client_num)
# futs = []
# for cl in clients:
#     fu = pool.submit(send, cl)
#     futs.append(fu)

# for fu in futs:
#     count = fu.result()
#     total_count += count
count = send(client)
total_count += count

print(f'Total number of calls: {total_count}')
