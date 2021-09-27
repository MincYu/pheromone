# Getting Started on AWS

Pheromone depends on [Kubernetes](http://kubernetes.io). This tutorial will walk you through setting up the required dependencies to run the Kubernetes CLI (`kubectl`) and [kops](https://github.com/kubernetes/kops) (a tool to create & manage Kubernetes clusters on public cloud infrastructure).

### Prerequisites

We assume you are running inside an EC2 linux VM on AWS, where you have Python3 installed (preferably Python3.6 or later -- we have not tested with earlier versions).
AWS has default quotas on resources that can be allocated for accounts. The cluster to create in this doc will exceed the default vCPU limit(32) for a regular AWS account. Please make sure this limit is lifted before proceeding. 
[read more](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/ec2-resource-limits.html). 

### Step 0: Downloading Pheromone on your VM

* Run the following commands to clone the Pheromone and configure the `PHERO_HOME` environment variable:

```bash

git clone https://github.com/MincYu/pheromone.git

cd pheromone
export PHERO_HOME=$(pwd)
cd deploy/cluster
```

### Step 1: Installing `kubectl`, `kops`, & friends on your VM

* Install the `kubectl` binary using the Kubernetes documentation, found [here](https://kubernetes.io/docs/tasks/tools/install-kubectl). Don't worry about setting up your kubectl configuration yet.
* Install the `kops` binary -- documentation found [here](https://github.com/kubernetes/kops/blob/master/docs/install.md)
* Install a variety of Python dependencies: `pip3 install awscli boto3 kubernetes`
* Download and install the AWS CLI [here](https://docs.aws.amazon.com/cli/latest/userguide/install-cliv2.html).

### Step 2: Configuring `kops`

* `kops` requires an IAM group and user with permissions to access EC2, Route53, etc. You can find the commands to create these permissions [here](https://github.com/kubernetes/kops/blob/master/docs/getting_started/aws.md#aws). Make sure that you capture the Access Key ID and Secret Access Key for the `kops` IAM user and set them as environmnent variables (`AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`) and pass them into `aws configure`, as described in the above link.
* `kops` also requires an S3 bucket for state storage. More information about configuring this bucket can be found [here](https://github.com/kubernetes/kops/blob/master/docs/getting_started/aws.md#cluster-state-storage).
* Create a service linked role for the Elastic Load Balancer (ELB) service: `aws iam create-service-linked-role --aws-service-name "elasticloadbalancing.amazonaws.com"`.
* Finally, in order to access the cluster, you will need a domain name to point to. Currently, we have only tested our setup scripts with domain names registered in Route53. `kops` supports a variety of DNS settings, which you can find more information about [here](https://github.com/kubernetes/kops/blob/master/docs/getting_started/aws.md#configure-dns).

### Step 3: Odds and ends

* Our cluster creation scripts depend on three environment variables: `PHERO_HOME`, `PHERO_CLUSTER_NAME`, and `KOPS_STATE_STORE`. Set `PHERO_HOME` as described in Step 0. Set the `PHERO_CLUSTER_NAME` variable to the name of the Route53 domain that you're using (see Footnote 2 if you are not using a Route53 domain -- you will need to modify the cluster creation scripts). Set the `KOPS_STATE_STORE` variable to the S3 URL of S3 bucket you created in Step 2 (e.g., `s3://pheromone-kops-state-store`).
* As described in Footnote 1, make sure that your `$PATH` variable includes the path to the `aws` CLI tool. You can check if its on your path by running `which aws` -- if you see a valid path, then you're set.
* As descried in Step 2, make sure you have run `aws configure` and set your region (by default, we use `us-east-1`) and the access key parameters for the kops user created in Step 2.

### Step 4: Creating your first cluster

You're now ready to create your first cluster. To start off, we'll create a tiny cluster, with one memory tier node and one routing node. From the `$PHERO_HOME/cluster/` directory, run `python3 -m deploy.cluster.create_cluster -m 1 -r 1 -c 1 -f 1`. This will take about 10-15 minutes to run. Once it's finished, you will see the URL of two AWS [ELB](https://aws.amazon.com/elasticloadbalancing/)s, which can be used to interact with the Anna KVS and Pheromone, respectively.