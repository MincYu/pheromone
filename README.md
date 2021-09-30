# Pheromone

Pheromone is a serverless platform for expressive, ease-of-use, and high-performance function interactions.
Pheromone applies two-level distributed scheduling for low-latency function invocations, and performs zero-copy data exchange using shared memory in each worker node.

![Pheromone Architecture](https://github.com/MincYu/pheromone/blob/main/architecture.jpg?raw=true)

The key design of Pheromone lies in data-centric function orchestration, which lets data trigger functions by making the consuming patterns of intermediate data (i.e., function results) explicit.
Please see [our paper](https://arxiv.org/abs/2109.13492) for more details.

## Getting Started

Pheromone runs on a Kubenetes cluster.
Please refer to deploy/cluster for details.

## Acknowledgement

- Pheromone follows the cluster settings of [Cloudburst](https://github.com/hydro-project/cloudburst), where it applies the same way to deploy the cluster and uses [Anna](https://github.com/hydro-project/anna) as the durable key-value store. 
- Pheromone uses a high-performance [C++ IPC library](https://github.com/mutouyun/cpp-ipc) for shared-memory based data sharing. We slightly modify the code and place this in common/shm-ipc as a sub module