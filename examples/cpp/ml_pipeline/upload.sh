
set -x
func_nodes=$(kubectl get pod | grep func | cut -d " " -f 1 | tr -d " ")
for i in `seq 1 $1`
do 
  cp label_image.so label_image$i.so
  for pod in ${func_nodes}; do
    kubectl cp label_image$i.so $pod:/dev/shm/ -c local-sched
  done
  rm label_image$i.so
done

for pod in ${func_nodes}; do
  kubectl cp resize.so $pod:/dev/shm -c local-sched
  kubectl cp average.so $pod:/dev/shm -c local-sched
done

for pod in ${func_nodes}; do
  kubectl cp grace_hopper.jpg $pod:/dev/shm -c local-sched
done