#!/bin/bash

cd $PHERO_HOME/
rm -rf conf
mkdir -p conf

IP=`ifconfig eth0 | grep 'inet' | grep -v inet6 | sed -e 's/^[ \t]*//' | cut -d' ' -f2`

cd build && make -j4 && cd ..
touch conf/config.yml
echo -e "ip: $IP" | tee -a conf/config.yml

APP_NUM=$1
REQ_NUM=$2
# SLEEP=$3
CLIENT_NUM=$3
echo -e "management: $MANAGEMENT" | tee -a conf/config.yml
echo -e "app: $APP_NUM" | tee -a conf/config.yml
echo -e "req: $REQ_NUM" | tee -a conf/config.yml
# echo -e "sleep: $SLEEP" | tee -a conf/config.yml

if [[ -n "$CLIENT_NUM" ]]; then
    for i in in $(seq 1 $CLIENT_NUM); do
        ./build/target/client &
    done
    wait
else
    ./build/target/client
fi

