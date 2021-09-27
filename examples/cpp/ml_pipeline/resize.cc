#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <chrono>
#include <string>
#include <iostream>
#include "cpp_function.hpp"
using namespace std;
using namespace cv;

extern "C" {
  int handle(UserLibraryInterface* library, int arg_size, char** arg_values){
    auto func_start_t = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
    int image_id_ = stoi(arg_values[0]);
    std::string image_id = std::to_string(image_id_);
    while (image_id.size() < 3) image_id = "0" + image_id;

    string file_name = "/dev/shm/grace_hopper.jpg";
    int width = 224;
    int height = 224;
    int channel = 3;
    cv::Mat img = cv::imread(file_name.c_str());
    cv::resize(img, img, cv::Size(height, height));
    img.convertTo(img, CV_32FC3, 1/255.0);

    vector<cv::Mat> channels;
    cv::split(img, channels);

    auto img_obj = library->create_object( width * height * channel * 4 );
    float * resized_img = reinterpret_cast<float *>(img_obj->get_value());
    int index = 0;
    for (int c = 2; c >= 0; --c) { // R,G,B
        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                resized_img[index] = channels[c].at<float>(h, w); // R->G->B
                index++;
            }
        }
    }
    library->send_object(img_obj);
    auto func_end_t = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
    std::cout << "Preprocess function start: " << func_start_t <<  ", end: " << func_end_t << std::endl;
    return 0;
  }
}
