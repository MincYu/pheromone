# set -x

log_dir=/home/ubuntu/event_logs
# rm -r $log_dir
mkdir -p $log_dir
prefix=$1

coord_nodes=$(kubectl get pod | grep coord | cut -d " " -f 1 | tr -d " ")
for pod in ${coord_nodes[@]}; do
    kubectl exec -it ${pod} -- cat ephe-store/log_coordinator_0.txt | grep App &> $log_dir/${prefix}k_coord.txt
done

func_nodes=$(kubectl get pod | grep func | cut -d " " -f 1 | tr -d " ")

for pod in ${func_nodes[@]}; do
    kubectl exec -it ${pod} -c local-sched -- cat ephe-store/log_scheduler_0.txt &> $log_dir/${prefix}k_sched_${pod}.txt
    for func_id in $(seq 1 20); do
        result=`kubectl exec -it ${pod} -c function-${func_id} -- cat ephe-store/log_executor.txt | grep 'Execute count'`
        count=`echo $result | grep 'count' | wc -l`
        if [[ $count > '0' ]]; then
            echo $result &> $log_dir/${prefix}k_exec_${pod}_${func_id}.txt
        fi
    done
done