#include "module.h"
#include "util.h"
#include <opencv2/opencv.hpp>
#include <iostream>

/* Define custom error codes */
enum ERROR_CODE {
    MALLOC_ERR = 1,
    OPENCV_ERR = 2,
    OPENCV_RES_ERR = 3,
    INVALID_INPUT = 7,
    INVALID_INPUT_VALUES = 8,
    INVALID_NEW_INPUT_VALUES = 9,
    INVALID_TARGET_SIZE = 10,
};

/* START MODULE IMPLEMENTATION */
void module()
{
    /* Get number of images in input batch */
    int num_images = get_input_num_images();

    if (num_images <= 0){
        signal_error_and_exit(INVALID_INPUT);
    }

    int target_size = get_param_int("target_size");
    bool save_og = get_param_bool("save_og_image");

    if (target_size <= 0){
        signal_error_and_exit(INVALID_TARGET_SIZE);
    }

    /* Example code for iterating a pixel value at a time */
    for (int i = 0; i < num_images; ++i)
    {
        /* Get input image metadata */
        Metadata *input_meta = get_metadata(i);
        int height = input_meta->height;
        int width = input_meta->width;
        int channels = input_meta->channels;
        int timestamp = input_meta->timestamp;
        int bits_pixel = input_meta->bits_pixel;
        char *camera = input_meta->camera;
        int obid = input_meta->obid;

        if (height <= 0 || width <= 0 || channels <= 0){
            signal_error_and_exit(INVALID_INPUT_VALUES);
        }

        unsigned char *input_image_data;
        size_t size = get_image_data(i, &input_image_data);

    
        // Calculate scale to fit within target_size while preserving aspect ratio
        double scale = std::min(static_cast<double>(target_size) / width, 
                            static_cast<double>(target_size) / height);
    
        int new_width = static_cast<int>(width * scale);
        int new_height = static_cast<int>(height * scale);

        if (new_height <= 0 || new_width <= 0){
            signal_error_and_exit(INVALID_NEW_INPUT_VALUES);
        }

        int cv_depth = (bits_pixel == 8) ? CV_8U : 
                       (bits_pixel == 16) ? CV_16U : 
                       (signal_error_and_exit(INVALID_INPUT_VALUES), CV_8U);

        int cv_type = (channels == 1) ? CV_MAKETYPE(cv_depth, 1) :
                      (channels == 3) ? CV_MAKETYPE(cv_depth, 3) :
                      (signal_error_and_exit(INVALID_INPUT_VALUES), CV_8UC1);

        cv::Mat rawImage(height, width, cv_type, input_image_data);

        if (rawImage.empty()) {
            signal_error_and_exit(OPENCV_ERR);
        }

        if (save_og)
        {
            size_t og_size = rawImage.total() * rawImage.elemSize();
            unsigned char *og_copy = (unsigned char *)malloc(og_size);
            if (og_copy == NULL) {
                signal_error_and_exit(MALLOC_ERR);
            }
        
            memcpy(og_copy, rawImage.data, og_size);
        
            Metadata og_meta = METADATA__INIT;
            if (clone_metadata(input_meta, &og_meta) != 0) {
                signal_error_and_exit(MALLOC_ERR);
            }

            og_meta.size = og_size;  // Only thing that changes
            add_custom_metadata_int(&og_meta, "saved_original", 1);
        
            append_result_image(og_copy, og_size, &og_meta);
            free(og_copy);
        }

        cv::Mat thumbnailImage;
        cv::resize(rawImage, thumbnailImage, cv::Size(new_width, new_height), 0, 0, cv::INTER_CUBIC);

        if (thumbnailImage.empty() || thumbnailImage.data == NULL){
            signal_error_and_exit(OPENCV_RES_ERR);
        }

        /* Calculate output image size */
        size_t output_size = thumbnailImage.total() * thumbnailImage.elemSize();

        /* Allocate memory for output image data */
        unsigned char *output_image_data = (unsigned char *)malloc(output_size);
        
        /* Check for malloc error */
        if (output_image_data == NULL)
        {
            signal_error_and_exit(MALLOC_ERR);
        }
        
        /* Copy demosaiced data to output buffer */
        memcpy(output_image_data, thumbnailImage.data, output_size);

        Metadata new_meta = METADATA__INIT;
            if (clone_metadata(input_meta, &new_meta) != 0)
            {
                signal_error_and_exit(MALLOC_ERR);
            }
        
        /* Create output image metadata */
         new_meta.size = output_size;
        new_meta.width = new_width;
        new_meta.height = new_height;

        /* Add custom metadata for demosaicing info */
        add_custom_metadata_int(&new_meta,"resized", target_size);
 
        /* Append the processed image to the result batch */
        append_result_image(output_image_data, output_size, &new_meta);
        
        /* Free allocated memory */
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
