#!/bin/bash
set -x
cd $PHERO_HOME/client/python
rm -rf proto
mkdir -p proto
cd proto
touch __init__.py

cd $PHERO_HOME/common/proto
protoc -I=. --python_out=../../client/python/proto/ common.proto kvs.proto operation.proto platform.proto

cd $PHERO_HOME/client/python/proto
if [[ "$OSTYPE" = "darwin"* ]]; then
  sed -i "" "s/import common_pb2/from . import common_pb2/g" kvs_pb2.py
  sed -i "" "s/import common_pb2/from . import common_pb2/g" operation_pb2.py
else 
  sed -i "s|import common_pb2|from . import common_pb2|g" kvs_pb2.py
  sed -i "s|import common_pb2|from . import common_pb2|g" operation_pb2.py
fi