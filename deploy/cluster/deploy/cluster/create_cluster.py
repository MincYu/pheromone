#!/usr/bin/env python3

#  Copyright 2019 U.C. Berkeley RISE Lab
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

import argparse
import os

import boto3

from deploy.cluster.add_nodes import batch_add_nodes
from deploy.cluster import util

BATCH_SIZE = 100

ec2_client = boto3.client('ec2', os.getenv('AWS_REGION', 'us-east-1'))

def create_cluster(mem_count, ebs_count, func_count, coord_count,
                   route_count, sender_count, cfile, ssh_key, cluster_name,
                   kops_bucket, aws_key_id, aws_key):

    if 'PHERO_HOME' not in os.environ:
        raise ValueError('PHERO_HOME environment variable must be set')
    prefix = os.path.join(os.environ['PHERO_HOME'], 'deploy/cluster/deploy/cluster')

    util.run_process(['./create_cluster_object.sh', kops_bucket, ssh_key])

    client, apps_client = util.init_k8s()

    print('Creating management pods...')
    management_spec = util.load_yaml('yaml/pods/management-pod.yml', prefix)
    env = management_spec['spec']['containers'][0]['env']

    util.replace_yaml_val(env, 'AWS_ACCESS_KEY_ID', aws_key_id)
    util.replace_yaml_val(env, 'AWS_SECRET_ACCESS_KEY', aws_key)
    util.replace_yaml_val(env, 'KOPS_STATE_STORE', kops_bucket)
    util.replace_yaml_val(env, 'PHERO_CLUSTER_NAME', cluster_name)

    client.create_namespaced_pod(namespace=util.NAMESPACE, body=management_spec)

    # Waits until the management pod starts to move forward -- we need to do
    # this because other pods depend on knowing the management pod's IP address.
    management_ip = util.get_pod_ips(client, 'role=management', is_running=True)[0]

    print('Creating management service...')
    service_spec = util.load_yaml('yaml/services/management.yml', prefix)
    if util.get_service_address(client, 'management-service') is None:
        client.create_namespaced_service(namespace=util.NAMESPACE, body=service_spec)


    management_podname = management_spec['metadata']['name']
    kcname = management_spec['spec']['containers'][0]['name']

    os.system('cp %s anna-config.yml' % cfile)
    kubecfg = os.path.join(os.environ['HOME'], '.kube/config')
    util.copy_file_to_pod(client, kubecfg, management_podname, '/root/.kube/',
                          kcname)
    util.copy_file_to_pod(client, ssh_key, management_podname, '/root/.ssh/',
                          kcname)
    util.copy_file_to_pod(client, ssh_key + '.pub', management_podname,
                          '/root/.ssh/', kcname)
    util.copy_file_to_pod(client, 'anna-config.yml', management_podname,
                          '/hydro/anna/conf/', kcname)

    # Start the monitoring pod.
    mon_spec = util.load_yaml('yaml/pods/monitoring-pod.yml', prefix)
    util.replace_yaml_val(mon_spec['spec']['containers'][0]['env'], 'MGMT_IP',
                          management_ip)
    client.create_namespaced_pod(namespace=util.NAMESPACE, body=mon_spec)

    # Wait until the monitoring pod is finished creating to get its IP address
    # and then copy KVS config into the monitoring pod.
    util.get_pod_ips(client, 'role=monitoring', is_running=True)
    util.copy_file_to_pod(client, 'anna-config.yml',
                          mon_spec['metadata']['name'],
                          '/hydro/anna/conf/',
                          mon_spec['spec']['containers'][0]['name'])
    os.system('rm anna-config.yml')

    print('Creating %d routing nodes...' % (route_count))
    batch_add_nodes(client, apps_client, cfile, ['routing'], [route_count], BATCH_SIZE, prefix)
    util.get_pod_ips(client, 'role=routing')

    print('Creating %d memory, %d ebs node(s)...' %
          (mem_count, ebs_count))
    batch_add_nodes(client, apps_client, cfile, ['memory', 'ebs'], [mem_count,
                                                                    ebs_count],
                    BATCH_SIZE, prefix)

    print('Creating routing service...')
    service_spec = util.load_yaml('yaml/services/routing.yml', prefix)
    # Only create the routing service if it isn't up already
    # (e.g. from a previous execution of the script).
    if util.get_service_address(client, 'routing-service') is None:
        client.create_namespaced_service(namespace=util.NAMESPACE,
                                         body=service_spec)

    print('Adding %d coordinator nodes...' % (coord_count))
    batch_add_nodes(client, apps_client, cfile, ['coordinator'], [coord_count], BATCH_SIZE, prefix)
    util.get_pod_ips(client, 'role=coordinator')

    coord_ips = util.get_pod_ips(client, 'role=coordinator')
    print(f'Coordinator ips: {coord_ips}')

    print('Adding %d function nodes...' % (func_count))
    batch_add_nodes(client, apps_client, cfile, ['function'], [func_count], BATCH_SIZE, prefix)

    print('Adding %d sender/client nodes...' % (sender_count))
    batch_add_nodes(client, apps_client, cfile, ['sender'], [sender_count], BATCH_SIZE, prefix)

    print('Finished creating all pods...')
    # os.system('touch setup_complete')
    # util.copy_file_to_pod(client, 'setup_complete', management_podname, '/hydro',
    #                       kcname)
    # os.system('rm setup_complete')

    sg_name = 'nodes.' + cluster_name
    sg = ec2_client.describe_security_groups(
          Filters=[{'Name': 'group-name',
                    'Values': [sg_name]}])['SecurityGroups'][0]

    print('Authorizing ports for routing service...')

    permission = [
        {
        'FromPort': 7800,
        'IpProtocol': 'tcp',
        'ToPort': 7950,
        'IpRanges': [{
            'CidrIp': '0.0.0.0/0'
        }]},
        {
        'FromPort': 6450,
        'IpProtocol': 'tcp',
        'ToPort': 6453,
        'IpRanges': [{
            'CidrIp': '0.0.0.0/0'
        }]},
        {
        'FromPort': 6200,
        'IpProtocol': 'tcp',
        'ToPort': 6203,
        'IpRanges': [{
            'CidrIp': '0.0.0.0/0'
        }]},
        {
        'FromPort': 6001,
        'IpProtocol': 'tcp',
        'ToPort': 6002,
        'IpRanges': [{
            'CidrIp': '0.0.0.0/0'
        }]},
        {
        'FromPort': 5000,
        'IpProtocol': 'tcp',
        'ToPort': 5080,
        'IpRanges': [{
            'CidrIp': '0.0.0.0/0'
        }]}
    ]

    ec2_client.authorize_security_group_ingress(GroupId=sg['GroupId'],
                                                IpPermissions=permission)

    routing_svc_addr = util.get_service_address(client, 'routing-service')
    management_svc_addr = util.get_service_address(client, 'management-service')
    print('Anna the kvs service can be accessed here: \n\t%s' %
          (routing_svc_addr))
    print('Pheromone can be accessed here: \n\t%s' %
          (management_svc_addr))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='''Creates a Hydro cluster
                                     using Kubernetes and kops. If no SSH key
                                     is specified, we use the default SSH key
                                     (~/.ssh/id_rsa), and we expect that the
                                     correponding public key has the same path
                                     and ends in .pub.

                                     If no configuration file base is
                                     specified, we use the default
                                     ($PHERO_HOME/conf/anna-base.yml).''')

    parser.add_argument('-m', '--memory', nargs=1, type=int, metavar='M',
                        help='The number of memory nodes to start with ' +
                        '(required)', dest='memory', required=True)
    parser.add_argument('-r', '--routing', nargs=1, type=int, metavar='R',
                        help='The number of routing  nodes in the cluster ' +
                        '(required)', dest='routing', required=True)
    parser.add_argument('-f', '--function', nargs=1, type=int, metavar='F',
                        help='The number of function nodes to start with ' +
                        '(required)', dest='function', required=True)
    parser.add_argument('-c', '--coordinator', nargs=1, type=int, metavar='C',
                        help='The number of coordinator nodes to start with ' +
                        '(required)', dest='coordinator', required=True)
    parser.add_argument('-s', '--sender', nargs='?', type=int, metavar='S',
                        help='The number of client nodes to start with',
                        dest='sender', default=0)
    parser.add_argument('-e', '--ebs', nargs='?', type=int, metavar='E',
                        help='The number of EBS nodes to start with ' +
                        '(optional)', dest='ebs', default=0)
    parser.add_argument('--conf', nargs='?', type=str,
                        help='The configuration file to start the cluster with'
                        + ' (optional)', dest='conf',
                        default=os.path.join(os.getenv('PHERO_HOME', '../..'),
                                             'conf/anna-base.yml'))
    parser.add_argument('--ssh-key', nargs='?', type=str,
                        help='The SSH key used to configure and connect to ' +
                        'each node (optional)', dest='sshkey',
                        default=os.path.join(os.environ['HOME'],
                                             '.ssh/id_rsa'))

    cluster_name = util.check_or_get_env_arg('PHERO_CLUSTER_NAME')
    kops_bucket = util.check_or_get_env_arg('KOPS_STATE_STORE')
    aws_key_id = util.check_or_get_env_arg('AWS_ACCESS_KEY_ID')
    aws_key = util.check_or_get_env_arg('AWS_SECRET_ACCESS_KEY')

    args = parser.parse_args()
    create_cluster(args.memory[0], args.ebs, args.function[0],
                   args.coordinator[0], args.routing[0], args.sender,
                   args.conf, args.sshkey, cluster_name, kops_bucket,
                   aws_key_id, aws_key)
