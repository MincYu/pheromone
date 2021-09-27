set -x

g++ -std=c++11 -shared -fPIC -I../../../include/function_interface -c -o resize.o resize.cc
g++ `pkg-config opencv --cflags` -std=c++11 -shared -fPIC resize.o -o resize.so `pkg-config opencv --libs`

g++ -std=c++11 -shared -fPIC -I../../../../video_experiment/video_analysis/include/tensorflow/bazel-bin/tensorflow/include \
                            -I../../../../video_experiment/video_analysis/include/tensorflow/bazel-bin/tensorflow/include/src \
                            -I../../../include/function_interface -c -o label_image.o label_image.cc
g++ -std=c++11 -shared -fPIC -L../../../../video_experiment/video_analysis/lib label_image.o -ltensorflow_cc -o label_image.so

g++ -std=c++11 -shared -fPIC -I../../../include/function_interface -c -o average.o average.cc
g++ -std=c++11 -shared -fPIC average.o -o average.so
