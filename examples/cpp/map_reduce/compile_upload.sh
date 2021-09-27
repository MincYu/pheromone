set -x

mapper_name=$1
reducer_name=$2

if [[ -z $mapper_name ]]; then
    mapper_name=mapper
fi

if [[ -z $reducer_name ]]; then
    reducer_name=reducer
fi

SO_COMPILER=g++
FLAGS=""
${SO_COMPILER} -shared -fPIC -I../../../include/function_interface -o ${mapper_name}.so mapper.cpp
${SO_COMPILER} -shared -fPIC -I../../../include/function_interface -o ${reducer_name}.so reducer.cpp
# ${SO_COMPILER} -I../../../include/function_interface mock_executor.cpp -o executor -ldl

func_nodes=$(kubectl get pod | grep func | cut -d " " -f 1 | tr -d " ")
for pod in ${func_nodes[@]}; do
    kubectl cp ${mapper_name}.so $pod:/dev/shm -c local-sched &
    kubectl cp ${reducer_name}.so $pod:/dev/shm -c local-sched &
done

wait