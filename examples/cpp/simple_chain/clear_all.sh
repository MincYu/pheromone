# set -x

nodes=$(kubectl get pod | grep management | cut -d " " -f 1 | tr -d " ")
for pod in ${nodes[@]}; do
    kubectl exec -it ${pod} -- pkill -9 manager
done

nodes=$(kubectl get pod | grep coord | cut -d " " -f 1 | tr -d " ")
for pod in ${nodes[@]}; do
    kubectl exec -it ${pod} -- pkill -9 coordinator
done

nodes=$(kubectl get pod | grep functi | cut -d " " -f 1 | tr -d " ")
for pod in ${nodes[@]}; do
    kubectl exec -it ${pod} -c local-sched -- pkill -9 scheduler &> /dev/null &
    # kubectl exec -it ${pod} -c local-sched -- sh -c 'cd /dev/shm/ && ls __IPC_SHM__* | tr "\n" "\0" | xargs -0 rm' &> /dev/null &
    for func_id in $(seq 1 20); do
        kubectl exec -it $pod -c function-${func_id} -- pkill -9 executor &> /dev/null &
    done
done

echo "waiting clear"
wait