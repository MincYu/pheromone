set -x

func_nodes=$(kubectl get pod | grep func | cut -d " " -f 1 | tr -d " ")
for pod in ${func_nodes[@]}; do
    kubectl cp preprocess.so $pod:/dev/shm -c local-sched &
    kubectl cp get_company.so $pod:/dev/shm -c local-sched &
    kubectl cp count.so $pod:/dev/shm -c local-sched &
done

wait