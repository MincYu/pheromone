#!/bin/bash

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

install_protobuf() {

  # If the protobuf directory is nonexistent or is empty, then we will
  # redownload and recompile. Otherwise, we will skip this step and simply
  # reinstall the already compiled version.
  if [ ! -d "$PROTOBUF_DIR" ] || [ -z "$(ls -A $PROTOBUF_DIR)" ] ; then
    rm -rf $PROTOBUF_DIR

    wget https://github.com/google/protobuf/releases/download/v${PROTOBUF_VERSION}/protobuf-all-${PROTOBUF_VERSION}.zip
    unzip protobuf-all-${PROTOBUF_VERSION}.zip > /dev/null 2>&1
    mv protobuf-${PROTOBUF_VERSION} $PROTOBUF_DIR
    rm protobuf-all-${PROTOBUF_VERSION}.zip

    cd $PROTOBUF_DIR && ./autogen.sh
    cd $PROTOBUF_DIR && ./configure CXX=clang++ CXXFLAGS='-std=c++11 -stdlib=libc++ -O3 -g'

    cd $PROTOBUF_DIR && make -j4
  fi

  cd $PROTOBUF_DIR && sudo make install
  cd $PROTOBUF_DIR && sudo ldconfig
}

install_protoc() {
  # Idea copied from https://github.com/grpc/grpc-go/blob/master/vet.sh
  pushd /home/travis
  wget https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOBUF_VERSION}/protoc-${PROTOBUF_VERSION}-linux-x86_64.zip
  unzip protoc-${PROTOBUF_VERSION}-linux-x86_64.zip
  protoc --version
  popd
}

install_lcov() {
  wget http://sourceforge.net/projects/ltp/files/Coverage%20Analysis/LCOV-${LCOV_VERSION}/lcov-${LCOV_VERSION}.tar.gz
  tar xvzf lcov-${LCOV_VERSION}.tar.gz > /dev/null 2>&1
  rm -rf lcov-${LCOV_VERSION}.tar.gz

  cd $LCOV_DIR && sudo make install
  which lcov
  lcov -v
}

sudo apt-get update
sudo apt-get install -y build-essential autoconf automake libtool curl make unzip pkg-config wget
sudo apt-get install -y libc++-dev libc++abi-dev awscli jq python3-pip

sudo ln -s $(which clang) /usr/bin/clang
sudo ln -s $(which clang++) /usr/bin/clang++

# if the protobuf directory doesn't exist or is empty

if [ ! -z "$PROTOC_ONLY" ]; then
  install_protoc
else
  install_protobuf
fi

cd $HOME

# Only install lcov if we have the lcov version environment variable set -- i.e., if we're using it
# in a CPP project. Otherwise, skip this.
if [[ ! -z "$LCOV_VERSION" ]]; then
  LCOV_DIR="lcov-${LCOV_VERSION}"
  install_lcov
fi

# Install Python library dependencies.
sudo pip3 install pycodestyle coverage codecov

