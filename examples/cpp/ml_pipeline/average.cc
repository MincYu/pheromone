#include <iostream>
#include <chrono>
#include <vector>
#include "cpp_function.hpp"
extern "C" {
 int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
    auto func_start_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    int size = reinterpret_cast<int *>(arg_values[0])[0]-1;
    string out_bucket("output");
    string out_key("average");
    auto tensor_obj = library->create_object(out_bucket, out_key, size); 
    float * tensor_ptr = reinterpret_cast<float *>(tensor_obj->get_value());
    for (int i = 0; i < size; ++i) {
    float mean = 0.0;
          for (int j = 0; j < arg_size; j++) {
        float data_ = reinterpret_cast<float *>(arg_values[j])[i+1];
        mean += data_;
    }
    tensor_ptr[i] = mean/arg_size;
    }

    library->send_object(tensor_obj, true);
    auto func_end_t = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    std::cout << "Average function start: " << func_start_t << ", end: " << func_end_t << std::endl;
   return 0;
 }
}

