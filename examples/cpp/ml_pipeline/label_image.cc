
#include <fstream>
#include <utility>
#include <vector>

#include "tensorflow/cc/ops/const_op.h"
#include "tensorflow/cc/ops/image_ops.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/graph/default_device.h"
#include "tensorflow/core/graph/graph_def_builder.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/core/stringpiece.h"
#include "tensorflow/core/lib/core/threadpool.h"
#include "tensorflow/core/lib/io/path.h"
#include "tensorflow/core/lib/strings/stringprintf.h"
#include "tensorflow/core/platform/init_main.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/public/session.h"
#include "tensorflow/core/util/command_line_flags.h"

#include "cpp_function.hpp"


// These are all common classes it's handy to reference with no namespace.
using tensorflow::Flag;
using tensorflow::Tensor;
using tensorflow::Status;
using tensorflow::string;
using tensorflow::int32;
using namespace std;

// Reads a model graph definition from disk, and creates a session object you
// can use to run it.
Status LoadGraph(const string& graph_file_name,
                 std::unique_ptr<tensorflow::Session>* session) {
  tensorflow::GraphDef graph_def;
  Status load_graph_status =
      ReadBinaryProto(tensorflow::Env::Default(), graph_file_name, &graph_def);
  if (!load_graph_status.ok()) {
    return tensorflow::errors::NotFound("Failed to load compute graph at '",
                                        graph_file_name, "'");
  }
  session->reset(tensorflow::NewSession(tensorflow::SessionOptions()));
  Status session_create_status = (*session)->Create(graph_def);
  if (!session_create_status.ok()) {
    return session_create_status;
  }
  return Status::OK();
}

// First we load and initialize the model every time loading the .so.
string graph_path =
      "/video_experiment/video_analysis/data/mobilenet_v2_1.4_224_frozen.pb";
std::unique_ptr<tensorflow::Session> session;
Status load_graph_status = LoadGraph(graph_path, &session);

extern "C" {
  int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
    auto func_start_t = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
    float * img = reinterpret_cast<float *>(arg_values[0]);
    
    int32 input_width = 224;
    int32 input_height = 224;
    int32 input_mean = 0;
    int32 input_std = 255;
    string input_layer = "input:0";
    string output_layer = "MobilenetV2/Predictions/Reshape_1:0";
    
    if (!load_graph_status.ok()) {
      return 1;
    }

    // Get the image from disk as a float array of numbers, resized and normalized
    // to the specifications the main graph expects.
    std::vector<Tensor> resized_tensors;

    Tensor input1(
		  tensorflow::DT_FLOAT,
		  tensorflow::TensorShape({ 1,input_width, input_height, 3 }));

	  auto input_map = input1.tensor<float, 4>();
    int step = input_width * input_height;
		for (int c = 0; c < 3; ++c) {
      int index = 0;
      for (int y = 0; y < input_width; ++y) {
        for (int x = 0; x < input_height; ++x) {
          input_map(0, y, x, c) = img[index + c * step];
          index++;
        }
      }    
    }

    auto pre_t = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
    // Actually run the image through the model.
    std::vector<Tensor> outputs;
    Status run_status = session->Run({{input_layer, input1}},
                                    {output_layer}, {}, &outputs);
    if (!run_status.ok()) {
      LOG(ERROR) << "Running model failed: " << run_status;
      return 1;
    }

    Tensor t = outputs[0];

    int size = t.shape().dim_size(1);
    auto tmap = t.tensor<float, 2>();

    string tgt_func("average");
    auto tensor_obj = library->create_object( tgt_func, true, size * 4 + 4 );
    
    // auto tensor_obj = library->create_object("b_average", library->gen_unique_key(), size * 4 + 4 );
    int * out_ptr_ = reinterpret_cast<int *>(tensor_obj->get_value());

    out_ptr_[0] = size;
    int index_ = 1;
    float * out_ptr = reinterpret_cast<float *>(out_ptr_);
    for (int i = 0; i < size; i++){
      out_ptr[index_] = tmap(0, i);
      index_++;
    }
    library->send_object(tensor_obj);
    auto func_end_t = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::cout << "Inference function start: " << func_start_t << ", format: " << pre_t  << ", end: " << func_end_t << std::endl;
    return 0;
  }
}

