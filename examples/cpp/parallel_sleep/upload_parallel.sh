set -x

func_nodes=$(kubectl get pod | grep func | cut -d " " -f 1 | tr -d " ")
for pod in ${func_nodes[@]}; do
    kubectl cp sleep.so $pod:/dev/shm -c local-sched &
    kubectl cp parallel_join.so $pod:/dev/shm -c local-sched &
done

wait