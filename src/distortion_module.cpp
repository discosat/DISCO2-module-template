#include "module.h"
#include "util.h"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <fstream>

/* Define custom error codes */
enum ERROR_CODE {
    MALLOC_ERR = 1,
    OPENCV_ERR = 2,
    OPENCV_MAT_ERR = 3,
    INVALID_CAM_VALUES = 7,   
};

/* START MODULE IMPLEMENTATION */
void module()
{
    cv::Mat K_811;
    cv::Mat D_811;
    cv::Mat K_507;
    cv::Mat D_507;
    cv::Mat K = cv::Mat::zeros(3, 3, CV_64F);
    cv::Mat D = cv::Mat::zeros(1, 5, CV_64F);

    int num_images = get_input_num_images();

    for (int i = 0; i < num_images; ++i)
    {
        Metadata *input_meta = get_metadata(i);
        int height = input_meta->height;
        int width = input_meta->width;
        int channels = input_meta->channels;
        int timestamp = input_meta->timestamp;
        int bits_pixel = input_meta->bits_pixel;
        char *camera = input_meta->camera;
        int obid = input_meta->obid;

        if (!(std::string(camera) == "1800 U-811c" || std::string(camera) == "1800 U-507c")) {
            signal_error_and_exit(INVALID_CAM_VALUES);
            }

        //which one is 1 and 2?
        if (std::string(camera) == "1800 U-811c"){
            if(K_811.empty() || D_811.empty()){
                K_811 = cv::Mat(3, 3, CV_64F);
                D_811 = cv::Mat(1, 5, CV_64F);
                //load the K matrix
                K_811 = (cv::Mat_<double>(3,3) << 255523.216, 0.0, 1188.59959,
                                                  0.0, 253926.994, 1207.97101,
                                                  0.0, 0.0, 1.0);
                //load the D matrix
                D_811 = (cv::Mat_<double>(1,5) << -34.3467044, -0.047078292, 0.0, 0.0, -0.00000401687332);
            }
                K = K_811;
                D = D_811;
                if (K.empty() || K.rows != 3 || K.cols != 3) {
                    signal_error_and_exit(OPENCV_MAT_ERR);
                }
                if (D.empty() || D.rows != 1 || D.cols != 5) {
                    signal_error_and_exit(OPENCV_MAT_ERR);
                }
        } else if (std::string(camera) == "1800 U-507c"){
            if(K_507.empty() || D_507.empty()){
                K_507 = cv::Mat(3, 3, CV_64F);
                D_507 = cv::Mat(1, 5, CV_64F);
                //load the K matrix
                K_507 = (cv::Mat_<double>(3,3) << 2391.95935091607, 0.0, 1232.68706589328,
                                                  0.0, 2392.35647316894, 1023.73390593577,
                                                  0.0, 0.0, 1.0);
                //load the D matrix
                D_507 = (cv::Mat_<double>(1,5) << -0.114417559579259, 0.132523919498935, 0.0, 0.0, 0.0);
            }
                K = K_507;
                D = D_507;
                if (K.empty() || K.rows != 3 || K.cols != 3) {
                    signal_error_and_exit(OPENCV_MAT_ERR);
                }
                if (D.empty() || D.rows != 1 || D.cols != 5) {
                    signal_error_and_exit(OPENCV_MAT_ERR);
                }
        }
        
        unsigned char *input_image_data;
        size_t size = get_image_data(i, &input_image_data);

        cv::Mat input_image(height, width, (channels == 3) ? CV_16UC3 : CV_16UC1, input_image_data);

                if (input_image.empty() || input_image.data == NULL) {
            signal_error_and_exit(OPENCV_ERR);
        }

        cv::Mat undistorted_image;
        cv::undistort(input_image, undistorted_image, K, D);
        if (undistorted_image.empty() || undistorted_image.data == NULL) {
            signal_error_and_exit(OPENCV_ERR);
        }

        unsigned char *output_image_data = (unsigned char *)malloc(size);
        if (output_image_data == NULL)
        {
            signal_error_and_exit(MALLOC_ERR);
        }

        memcpy(output_image_data, undistorted_image.data, size);

        Metadata new_meta = METADATA__INIT;
        if (clone_metadata(input_meta, &new_meta) != 0)
        {
            signal_error_and_exit(MALLOC_ERR);
        }

        add_custom_metadata_bool(&new_meta, "distortion_corrected", true);

        /* Append the processed image to the result batch */
        append_result_image(output_image_data, size, &new_meta);

        free(input_image_data);
        free(output_image_data);
    }

}
/* END MODULE IMPLEMENTATION */

/* Main function of module (NO NEED TO MODIFY) */
ImageBatch run(ImageBatch *input_batch, ModuleParameterList *module_parameter_list, int *ipc_error_pipe)
{
    ImageBatch result_batch;
    result = &result_batch;
    input = input_batch;
    config = module_parameter_list;
    error_pipe = ipc_error_pipe;
    initialize();

    module();

    finalize();
    return result_batch;
}
