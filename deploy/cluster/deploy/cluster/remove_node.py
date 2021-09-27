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

from deploy.cluster import util

def remove_node(ip, ntype):
    client, _ = util.init_k8s()

    pod = util.get_pod_from_ip(client, ip)
    hostname = 'ip-%s.ec2.internal' % (ip.replace('.', '-'))

    prev_count = util.get_previous_count(client, ntype)

    util.run_process(['./delete_node.sh', hostname, ntype, str(prev_count), str(prev_count - 1)])