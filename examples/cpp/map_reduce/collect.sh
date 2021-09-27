# set -x

log_dir=/home/ubuntu/mr_logs
# rm -r $log_dir
mkdir -p $log_dir/details
prefix=$1

nodes=$(kubectl get pod | grep coord | cut -d " " -f 1 | tr -d " ")
for pod in ${nodes[@]}; do
    kubectl exec -it $pod -- cat ephe-store/log_coordinator_0.txt &> $log_dir/coord_${pod}.txt
done

func_nodes=$(kubectl get pod | grep func | cut -d " " -f 1 | tr -d " ")
for pod in ${func_nodes[@]}; do
    kubectl exec -it $pod -c local-sched -- cat ephe-store/log_scheduler_0.txt &> $log_dir/sched_${pod}.txt
    for func_id in $(seq 1 11); do
        result=`kubectl exec $pod -c function-${func_id} -- cat ephe-store/log_executor.txt`
        count=`echo $result | grep 'sort_map' | wc -l`
        if [[ $count > '0' ]]; then
            echo $result | grep 'sort_map' &> $log_dir/map_${pod}_${func_id}.txt
        fi
        count=`echo $result | grep 'sort_reduce' | wc -l`
        if [[ $count > '0' ]]; then
            echo $result | grep 'sort_reduce' &> $log_dir/reduce_${pod}_${func_id}.txt
        fi

        result=`kubectl logs $pod -c function-${func_id} | tail -1`
        count=`echo $result | grep 'Map' | wc -l`
        if [[ $count > '0' ]]; then
            echo $result | grep 'Map' &> $log_dir/details/map_${pod}_${func_id}.txt
        fi
        count=`echo $result | grep 'Reduce' | wc -l`
        if [[ $count > '0' ]]; then
            echo $result | grep 'Reduce' &> $log_dir/details/reduce_${pod}_${func_id}.txt
        fi
    done
done