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

# Only run make clang-format if there is a build directory and it is not empty
if [ -d "build" ] && [ ! -z "$(ls -A build)" ] ; then
  cd build && make clang-format && cd ..
fi

FILES=`git status --porcelain`
MODIFIED=`git status --porcelain | wc -l`

if [[ $MODIFIED -gt 0 ]]; then
  echo -e "clang-format check failed. The following files had style issues:\n"
  echo -e $FILES

  exit 1
else
  echo "clang-format check succeeded!"
fi

ERRORS=`find * | grep py | grep -v build | grep -v pb2 | grep -v txt | xargs pycodestyle`
MODIFIED=`echo $ERRORS | tr -d [:space:] | wc -l`

if [[ $MODIFIED -gt 0 ]]; then
  echo -e "pycodestyle check failed. These were the issues reported:\n"
  echo -e $ERRORS

  exit 1
else
  echo "pycodestyle check succeeded!"
  exit 0
fi
