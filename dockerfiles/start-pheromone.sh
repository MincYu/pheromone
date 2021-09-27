#!/bin/bash

cd $PHERO_HOME
rm -rf conf
mkdir -p conf

gen_yml_list() {
  IFS=' ' read -r -a ARR <<< $1
  RESULT=""

  for IP in "${ARR[@]}"; do
    RESULT=$"$RESULT        - $IP\n"
  done

  echo -e "$RESULT"
}

git pull origin main

IP=`ifconfig eth0 | grep 'inet' | grep -v inet6 | sed -e 's/^[ \t]*//' | cut -d' ' -f2`

cd build && make -j4 && cd ..
touch conf/config.yml

echo "Set the role as ${ROLE}"

if [[ "$ROLE" = "coordinator" ]]; then
  PUBLIC_IP=`curl ifconfig.me`
  echo -e "coord_threads: ${COORD_THREADS}" | tee -a conf/config.yml
  echo -e "io_threads: ${IO_THREADS}" | tee -a conf/config.yml
  echo -e "manager: ${MANAGEMENT}" | tee -a conf/config.yml
  echo -e "ip:" | tee -a conf/config.yml
  echo -e "    private: $IP" | tee -a conf/config.yml
  echo -e "    public: $PUBLIC_IP" | tee -a conf/config.yml
  echo -e "delay: ${DELAY}" | tee -a conf/config.yml

  while true; do
    ./build/target/coordinator

    if [[ "$?" = "1" ]]; then
      echo "Get error. Restarting"
    fi
  done

elif [[ "$ROLE" = "manager" ]]; then
  echo -e "ip: $IP" | tee -a conf/config.yml
  echo -e "external: $EXTERNAL_USER" | tee -a conf/config.yml
  mkdir -p /root/.kube
  while true; do
    ./build/target/manager

    if [[ "$?" = "1" ]]; then
      echo "Get error. Restarting"
    fi
  done

elif [[ "$ROLE" = "scheduler" ]]; then
  echo -e "threads:" | tee -a conf/config.yml
  echo -e "    coord: ${COORD_THREADS}" | tee -a conf/config.yml
  if [[ -n "$IO_THREADS" ]]; then
    echo -e "    io: ${IO_THREADS}" | tee -a conf/config.yml
  else 
    echo -e "    io: 1" | tee -a conf/config.yml
  fi

  echo -e "func_dir: /dev/shm/" | tee -a conf/config.yml
  echo -e "delay: ${DELAY}" | tee -a conf/config.yml
  if [[ -n "$SHARED" ]]; then
    echo -e "shared: ${SHARED}" | tee -a conf/config.yml
  else 
    echo  -e "shared: 1" | tee -a conf/config.yml
  fi

  if [[ -n "$FORWARD_REJECT" ]]; then
    echo -e "forward_or_reject: ${FORWARD_REJECT}" | tee -a conf/config.yml
  else 
    echo  -e "forward_or_reject: 0" | tee -a conf/config.yml
  fi

  if [[ -n "$EXECUTOR_NUM" ]]; then
    echo "Set executor number $EXECUTOR_NUM"
  else
    echo "No EXECUTOR_NUM found."
    exit 1
  fi

  echo -e "user:" | tee -a conf/config.yml
  echo -e "    ip: $IP" | tee -a conf/config.yml
  echo -e "    executor: $EXECUTOR_NUM" | tee -a conf/config.yml
  echo -e "    routing-elb: $ROUTE_ADDR" >> conf/config.yml

  LST=$(gen_yml_list "$COORD_IPS")
  echo -e "    coord:" | tee -a conf/config.yml
  echo -e "$LST" | tee -a conf/config.yml

  while true; do
    ./build/target/scheduler

    if [[ "$?" = "1" ]]; then
      echo "Get error. Restarting"
    fi
  done

elif [[ "$ROLE" = "executor" ]]; then
  cd /
  git clone https://github.com/Tingjia980311/video_experiment.git
  cd /video_experiment/video_analysis/lib
  wget https://github.com/Tingjia980311/video_experiment/releases/download/tensorflow_cc.so/libtensorflow_cc.so
  ln libtensorflow_cc.so libtensorflow_cc.so.2
  
  sed -i '$a\/video_experiment/video_analysis/lib/' /etc/ld.so.conf
  ldconfig
  
  cd /video_experiment/video_encoder
  wget http://archive.ubuntu.com/ubuntu/pool/universe/x/x264/libx264-152_0.152.2854+gite9a5903-2_amd64.deb
  dpkg -i libx264-152_0.152.2854+gite9a5903-2_amd64.deb
  rm libx264-152_0.152.2854+gite9a5903-2_amd64.deb

  cd $PHERO_HOME
  echo -e "ip: $IP" | tee -a conf/config.yml
  echo -e "thread_id: $THREAD_ID" | tee -a conf/config.yml
  echo -e "func_dir: /dev/shm/" | tee -a conf/config.yml

  if [[ -n "$WAIT" ]]; then
    echo -e "wait: $WAIT" | tee -a conf/config.yml
  fi

  while true; do
    ./build/target/executor

    if [[ "$?" = "1" ]]; then
      echo "Get error. Restarting"
    fi
  done

elif [[ "$ROLE" = "client" ]]; then
  echo -e "ip: $IP" | tee -a conf/config.yml
  echo -e "management: $MANAGEMENT" | tee -a conf/config.yml
  sleep infinity
fi
