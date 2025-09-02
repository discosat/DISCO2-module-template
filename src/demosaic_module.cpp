#include "module.h"
#include "util.h"
#include "globals.h"
#include <opencv2/opencv.hpp>

/* Define custom error codes */
enum ERROR_CODE {
    MALLOC_ERR = 1,
    OPENCV_ERR = 2,
};

/* START MODULE IMPLEMENTATION */
void module()
{
    /* Get number of images in input batch */
    int num_images = get_input_num_images();
    
    /* Process each image in the batch */
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
        
        /* Get input image data */
        unsigned char *input_image_data;
        size_t input_size = get_image_data(i, &input_image_data);
        
        /* Create OpenCV Mat for raw image (12-bit data in 16-bit container) */
        cv::Mat rawImage(height, width, CV_16UC1, (uint16_t*)input_image_data);
        
        /* Apply vertical flip to match camera orientation */
        cv::Mat flippedImage;
        cv::flip(rawImage, flippedImage, 0);  // 0 means vertical flip
        
        /* Perform demosaicing with GRBG pattern */
        cv::Mat demosaicedImage;
        cv::cvtColor(flippedImage, demosaicedImage, cv::COLOR_BayerGR2BGR);
        
        /* Calculate output image size */
        size_t output_size = demosaicedImage.total() * demosaicedImage.elemSize();
        
        /* Allocate memory for output image data */
        unsigned char *output_image_data = (unsigned char *)malloc(output_size);
        
        /* Check for malloc error */
        if (output_image_data == NULL)
        {
            signal_error_and_exit(MALLOC_ERR);
        }
        
        /* Copy demosaiced data to output buffer */
        memcpy(output_image_data, demosaicedImage.data, output_size);
        
        /* Create output image metadata */
        Metadata new_meta = METADATA__INIT;
        new_meta.size = output_size;
        new_meta.width = width;
        new_meta.height = height;
        new_meta.channels = 3; // BGR output
        new_meta.timestamp = timestamp;
        new_meta.bits_pixel = 8; // 8-bit output
        new_meta.camera = camera;
        new_meta.obid = obid;
        
        /* Add custom metadata for demosaicing info */
        add_custom_metadata_string(&new_meta, "processing", "demosaiced");
        add_custom_metadata_string(&new_meta, "bayer_pattern", "GRBG");
        add_custom_metadata_int(&new_meta, "output_channels", 3);
        add_custom_metadata_string(&new_meta, "orientation", "flipped_vertical");
        
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