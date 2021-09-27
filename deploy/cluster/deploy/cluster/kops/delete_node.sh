#!/bin/bash

# Safely evict the pods from the node that we are trying to delete
kubectl drain $1 --ignore-daemonsets --delete-local-data > /dev/null 2>&1

kubectl delete node $1 > /dev/null 2>&1

YML_FILE=yaml/igs/$2-ig.yml

sed "s|CLUSTER_NAME|$PHERO_CLUSTER_NAME|g" $YML_FILE > tmp.yml
sed -i "s|MAX_DUMMY|$3|g" tmp.yml
sed -i "s|MIN_DUMMY|$4|g" tmp.yml

kops replace -f tmp.yml --force > /dev/null 2>&1
rm tmp.yml

kops update cluster --name ${PHERO_CLUSTER_NAME} --yes > /dev/null 2>&1

ID=$(aws ec2 --region us-east-1 describe-instances --filter Name=private-dns-name,Values=$1 --query 'Reservations[].Instances[].InstanceId' --output text)

# Detach the ec2 instance associated with the node we just deleted.
# --should-decrement-desired-capacity is a mandatory flag that we need to specify to signal
# how the desired cluster size should change. For our purpose, the desired value does not matter.
aws autoscaling --region us-east-1 detach-instances --instance-ids $ID --auto-scaling-group-name $2-instances.$PHERO_CLUSTER_NAME --should-decrement-desired-capacity > /dev/null 2>&1

# Terminate the en2 instance.
aws ec2 --region us-east-1 terminate-instances --instance-ids $ID > /dev/null 2>&1

sed "s|CLUSTER_NAME|$PHERO_CLUSTER_NAME|g" $YML_FILE > tmp.yml
sed -i "s|MAX_DUMMY|$4|g" tmp.yml
sed -i "s|MIN_DUMMY|$4|g" tmp.yml

kops replace -f tmp.yml --force > /dev/null 2>&1
rm tmp.yml

kops update cluster --name ${PHERO_CLUSTER_NAME} --yes > /dev/null 2>&1