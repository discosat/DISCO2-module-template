#include "module.h"
#include "util.h"
#include "globals.h"
#include <opencv2/opencv.hpp>
#include <cmath>
#include <cstring>

// #include "dcp_colour.h"

/* Define custom error codes */
enum ERROR_CODE {
    MALLOC_ERR = 1,
    OPENCV_ERR = 2,
    INVALID_INPUT_ERR = 3,
    INVALID_CHANNELS_ERR = 4,
    INVALID_DEPTH_ERR = 5,
};
//wide angle
static const float CAM507_WB_3000K[3] = { 0.630950, 1.000000, 0.304648 };
static const float CAM507_WB_4000K[3] = { 0.509683, 1.000000, 0.454403 };
static const float CAM507_WB_5000K[3] = { 0.439863, 1.000000, 0.545025 };
static const float CAM507_WB_6500K[3] = { 0.627120, 1.000000, 0.490836 };
//tele
static const float CAM811_WB_3000K[3] = { 0.495784, 1.000000, 0.333513 };
static const float CAM811_WB_4000K[3] = { 0.409760, 1.000000, 0.455817 };
static const float CAM811_WB_5000K[3] = { 0.356347, 1.000000, 0.535889 };
static const float CAM811_WB_6500K[3] = { 0.486064, 1.000000, 0.487412 };

static const float* get_wb_gains(const char* camera_id, int temperature){
    if(strcmp(camera_id, "1800 U-507c") == 0){
        if(temperature == 3000) return CAM507_WB_3000K;
        if(temperature == 4000) return CAM507_WB_4000K;
        if(temperature == 5000) return CAM507_WB_5000K;
        if(temperature == 6500) return CAM507_WB_6500K;
    }
    if(strcmp(camera_id, "1800 U-811c") == 0){
        if(temperature == 3000) return CAM811_WB_3000K;
        if(temperature == 4000) return CAM811_WB_4000K;
        if(temperature == 5000) return CAM811_WB_5000K;
        if(temperature == 6500) return CAM811_WB_6500K;
    }
    return nullptr;
}

inline float apply_gamma(float v) {
    return std::pow(v, 1.0f / 2.2f);
}

inline float norm(float v){
    return(v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
}

/* START MODULE IMPLEMENTATION */
void module()
{
    /* Get number of images in input batch */
    int num_images = get_input_num_images();

    int temperature = get_param_int("temperature");

    Metadata *first_meta = get_metadata(0);
    const float* wb_gains = get_wb_gains(first_meta->camera, temperature);
    if (!wb_gains) {
        signal_error_and_exit(INVALID_INPUT_ERR);
    }        

    /* Example code for iterating a pixel value at a time */
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

        if (channels != 3) {
            signal_error_and_exit(INVALID_CHANNELS_ERR);
        }
        if (bits_pixel != 8 && bits_pixel != 16) {
            signal_error_and_exit(INVALID_DEPTH_ERR);
        }
       
        unsigned char *input_image_data;
        size_t size = get_image_data(i, &input_image_data);       

        /* Define temporary output image */
        unsigned char *output_image_data = (unsigned char *)malloc(size);

        memcpy(output_image_data, input_image_data, size);

        int cv_type = (bits_pixel == 8) ? CV_8UC3 : CV_16UC3;
        float scale = (bits_pixel == 8) ? (1.0f / 255.0f) : (1.0f / 65535.0f);
        float inv_scale = (bits_pixel == 8) ? 255.0f : 65535.0f;

        cv::Mat image(height, width, cv_type, output_image_data);

        if (image.empty()) {
            signal_error_and_exit(OPENCV_ERR);
        }

        if(bits_pixel == 8){
            for(int y = 0; y < image.rows; y++){
                cv::Vec3b* row = image.ptr<cv::Vec3b>(y);
                for(int x = 0; x < image.cols; x++){
                    float b = row[x][0] * scale * wb_gains[2];
                    float g = row[x][1] * scale * wb_gains[1];
                    float r = row[x][2] * scale * wb_gains[0];

                    row[x][0] = static_cast<uchar>(apply_gamma(norm(b)) * inv_scale + 0.5f);
                    row[x][1] = static_cast<uchar>(apply_gamma(norm(g)) * inv_scale + 0.5f);
                    row[x][2] = static_cast<uchar>(apply_gamma(norm(r)) * inv_scale + 0.5f);

                    }                   
                }
        } else {
            for(int y = 0; y < image.rows; y++){
                cv::Vec3w* row = image.ptr<cv::Vec3w>(y);
                for(int x = 0; x < image.cols; x++){
                    float b = row[x][0] * scale * wb_gains[2];
                    float g = row[x][1] * scale * wb_gains[1];
                    float r = row[x][2] * scale * wb_gains[0];

                    row[x][0] = static_cast<ushort>(apply_gamma(norm(b)) * inv_scale + 0.5f);
                    row[x][1] = static_cast<ushort>(apply_gamma(norm(g)) * inv_scale + 0.5f);
                    row[x][2] = static_cast<ushort>(apply_gamma(norm(r)) * inv_scale + 0.5f);

                    }                   
                }
            }

        /* Check for malloc error */
        if (output_image_data == NULL)
        {
            signal_error_and_exit(MALLOC_ERR);
        }

        /* Create image metadata before appending */
        Metadata new_meta = METADATA__INIT;
            if (clone_metadata(input_meta, &new_meta) != 0)
            {
                signal_error_and_exit(MALLOC_ERR);
            }     
        new_meta.size = size;
        new_meta.width = width;
        new_meta.height = height;
        new_meta.channels = channels;
        new_meta.timestamp = timestamp;
        new_meta.bits_pixel = bits_pixel;
        new_meta.camera = camera;
        new_meta.obid = obid;

        /* Add custom metadata for color correction info */
        add_custom_metadata_string(&new_meta, "correction", "color-corrected");
        add_custom_metadata_int(&new_meta, "temperature", temperature);

        /* Append the image to the result batch */
        append_result_image(output_image_data, size, &new_meta);

        /* Remember to free any allocated memory */
        free(input_image_data);
        free(output_image_data);
    }
}
/* END MODULE IMPLEMENTATION */

/* Main function of module (NO NEED TO MODIFY) */
extern "C" ImageBatch run(ImageBatch *input_batch, ModuleParameterList *module_parameter_list, int *ipc_error_pipe)
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
