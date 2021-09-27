set -x

func_nodes=$(kubectl get pod | grep func | cut -d " " -f 1 | tr -d " ")
for pod in ${func_nodes[@]}; do
    kubectl cp filter_loc.so $pod:/dev/shm -c local-sched &
    kubectl cp query_count.so $pod:/dev/shm -c local-sched &
    kubectl cp risk_level.so $pod:/dev/shm -c local-sched &
done

wait