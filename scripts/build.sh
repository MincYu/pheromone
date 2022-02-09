#!/bin/bash

args=( -j -b -t )
containsElement() {
  local e match="$1"
  shift
  for e; do [[ "$e" == "$match" ]] && return 0; done
  return 1
}

while getopts ":j:b:tg" opt; do
  case $opt in
   j )
     MAKE_THREADS=$OPTARG
     if containsElement $OPTARG "${args[@]}"
     then
       echo "Missing argument to flag $opt"
       exit 1
     else
       echo "make set to run on $OPTARG threads" >&2
     fi
     ;;
   b )
     TYPE=$OPTARG
     if containsElement $OPTARG "${args[@]}"
     then
       echo "Missing argument to flag $opt"
       exit 1
     else
       echo "build type set to $OPTARG" >&2
     fi
     ;;
   t )
     TEST="-DBUILD_TEST=ON"
     echo "Testing enabled..."
     ;;
   g )
     COMPILER="/usr/bin/g++"
     RUN_FORMAT=""
     echo "Compiler set to GNU g++..."
     ;;
   \? )
     echo "Invalid option: -$OPTARG" >&2
     exit 1
     ;;
  esac
done

if [[ -z "$MAKE_THREADS" ]]; then MAKE_THREADS=2; fi
if [[ -z "$TYPE" ]]; then TYPE=Release; fi
if [[ -z "$TEST" ]]; then TEST=""; fi
if [[ -z "$COMPILER" ]]; then
  COMPILER="/usr/bin/g++"
  RUN_FORMAT="yes"
fi

rm -rf build
mkdir build
cd build

cmake -std=c++14 "-GUnix Makefiles" -DCMAKE_BUILD_TYPE=$TYPE -DCMAKE_CXX_COMPILER=$COMPILER $TEST ..

make -j${MAKE_THREADS}

if [[ "$TYPE" = "Debug" ]] && [[ ! -z "$RUN_FORMAT" ]]; then
  make clang-format
fi
