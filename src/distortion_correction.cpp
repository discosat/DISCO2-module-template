#include "module.h"
#include "util.h"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <fstream>
#include "utils/logger.h"

namespace fs = std::filesystem;

/* Define custom error codes */
enum ERROR_CODE {
    MALLOC_ERR = 1,
    PLACEHOLDER = 2,
};

void load_calibration_data(cv::Mat &K, cv::Mat &D, Logger *logger) {
    logger_log(logger, LOG_INFO, "Loading calibration data");
    
    // Initialize K as a 3x3 matrix and D as a 1x5 matrix
    K = cv::Mat::zeros(3, 3, CV_64F);
    D = cv::Mat::zeros(1, 5, CV_64F);

    // Read camera matrix (K)
    std::ifstream K_file("camera_matrix.txt");
    if (!K_file.is_open()) {
        logger_log(logger, LOG_ERROR, "Failed to open camera_matrix.txt");
        return;
    }
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            K_file >> K.at<double>(i, j);
    K_file.close();
    logger_log(logger, LOG_INFO, "Loaded camera matrix");

    // Read distortion coefficients (D)
    std::ifstream D_file("distortion_coeffs.txt");
    if (!D_file.is_open()) {
        logger_log(logger, LOG_ERROR, "Failed to open distortion_coeffs.txt");
        return;
    }
    for (int i = 0; i < 5; ++i)
        D_file >> D.at<double>(0, i);
    D_file.close();
    logger_log(logger, LOG_INFO, "Loaded distortion coefficients");
}

/* START MODULE IMPLEMENTATION */
void module()
{
    fs::path dir("/home/root/logs/");
    fs::path file_name("distortion_" + std::to_string(std::time(0)) + ".txt");
    std::string full_path = (dir / file_name).string();
    Logger *logger = logger_create(full_path.c_str());

    logger_log_print(logger, LOG_INFO, "Distortion correction module started");

    cv::Mat K, D;
    load_calibration_data(K, D, logger);

    int num_images = get_input_num_images();
    logger_log(logger, LOG_INFO, ("Number of images: " + std::to_string(num_images)).c_str());

    for (int i = 0; i < num_images; ++i)
    {
        logger_log(logger, LOG_INFO, ("Processing image " + std::to_string(i)).c_str());

        Metadata *input_meta = get_metadata(i);
        int height = input_meta->height;
        int width = input_meta->width;
        int channels = input_meta->channels;
        int timestamp = input_meta->timestamp;
        int bits_pixel = input_meta->bits_pixel;
        char *camera = input_meta->camera;

        logger_log(logger, LOG_INFO, "Getting image data");
        unsigned char *input_image_data;
        size_t size = get_image_data(i, &input_image_data);
        logger_log(logger, LOG_INFO, "Got image data");

        logger_log(logger, LOG_INFO, "Creating OpenCV Mat from raw data");
        cv::Mat input_image(height, width, (channels == 3) ? CV_8UC3 : CV_8UC1, input_image_data);
        cv::Mat undistorted_image;

        logger_log(logger, LOG_INFO, "Applying distortion correction");
        cv::undistort(input_image, undistorted_image, K, D);

        unsigned char *output_image_data = (unsigned char *)malloc(size);
        if (output_image_data == NULL)
        {
            logger_log(logger, LOG_ERROR, "Memory allocation failed");
            signal_error_and_exit(MALLOC_ERR);
        }

        memcpy(output_image_data, undistorted_image.data, size);
        logger_log(logger, LOG_INFO, "Copied undistorted data to output buffer");

        Metadata new_meta = METADATA__INIT;
        new_meta.size = size;
        new_meta.width = width;
        new_meta.height = height;
        new_meta.channels = channels;
        new_meta.timestamp = timestamp;
        new_meta.bits_pixel = bits_pixel;
        new_meta.camera = camera;

        add_custom_metadata_bool(&new_meta, "distortion_corrected", true);

        logger_log(logger, LOG_INFO, "Appending result image");
        append_result_image(output_image_data, size, &new_meta);
        logger_log(logger, LOG_INFO, "Appended result image");

        logger_log(logger, LOG_INFO, "Freeing input memory");
        free(input_image_data);
        logger_log(logger, LOG_INFO, "Freeing output memory");
        free(output_image_data);

        logger_log(logger, LOG_INFO, ("Finished image " + std::to_string(i)).c_str());
    }

    logger_log_print(logger, LOG_INFO, "Distortion correction module finished");
    logger_flush(logger);
    logger_destroy(logger);
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
