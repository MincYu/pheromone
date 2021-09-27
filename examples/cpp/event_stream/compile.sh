set -x

SO_COMPILER=g++
FLAGS=""
for cpp_file in *.cpp; do
    filename="${cpp_file%.*}"
    ${SO_COMPILER} -shared -fPIC -I../../../include/function_interface -o ${filename}.so ${filename}.cpp
done