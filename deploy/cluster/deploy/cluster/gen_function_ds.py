import argparse
import os

parser = argparse.ArgumentParser()
parser.add_argument('--num', type=int, help="function number")
args = parser.parse_args()

num = args.num

prefix = '''apiVersion: apps/v1
kind: DaemonSet
metadata:
  name: function-nodes
  labels:
    role: function
spec:
  selector:
    matchLabels:
      role: function
  template:
    metadata:
      labels:
        role: function
    spec:
      nodeSelector:
        role: function
      restartPolicy: Always
      volumes:
      - emptyDir:
          medium: Memory
          sizeLimit: 10G
        name: cache-volume
      hostNetwork: true
      hostIPC: true
      containers:
'''

function_ds = ""
for i in range(num):
    function_ds += '''      - name: function-''' + str(i + 1) + '''
        resources:
          limits:
            cpu: "0.6"
            memory: "1G"
        image: cheneyyu/pheromone
        imagePullPolicy: Always
        env:
        - name: THREAD_ID
          value: "''' + str(i) + '''"
        - name: ROLE
          value: executor
        volumeMounts:
        - name: cache-volume
          mountPath: /dev/shm
'''

suffix = '''      - name: local-sched
        image: cheneyyu/pheromone
        imagePullPolicy: Always
        resources:
          limits:
            cpu: "1"
            memory: "1G"
        env:
        - name: COORD_IPS
          value: COORD_IPS_DUMMY
        - name: ROUTE_ADDR
          value: ROUTE_ADDR_DUMMY
        - name: ROLE
          value: scheduler
        - name: COORD_THREADS
          value: "1"
        - name: IO_THREADS
          value: "1"
        - name: EXECUTOR_NUM
          value: "''' + str(num) + '''"
        - name: DELAY
          value: "4000000000"
        volumeMounts:
        - name: cache-volume
          mountPath: /dev/shm
'''

with open('function-ds-test.yml', 'w') as f:
    f.write(prefix)
    f.write(function_ds)
    f.write(suffix)
