#include "module.h"
#include "util.h"
#include <opencv2/opencv.hpp>
#include <string.h>

//this one does not have normalization

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

    /* Retrieve module parameters by name (defined in config.yaml) (None for this module)*/

    /* Process each image in the batch */
    for (int i = 0; i < num_images; ++i)
    {
        Metadata *input_meta = get_metadata(i);
        int height = input_meta->height;
        int width = input_meta->width;
        int timestamp = input_meta->timestamp;
        int bits_pixel = input_meta->bits_pixel;
        char *camera = input_meta->camera;
        
        /* Validate input is single channel */
        if (input_meta->channels != 1) {
            signal_error_and_exit(OPENCV_ERR);
        }
        
        /* Validate bits_pixel */
        if (bits_pixel < 8 || bits_pixel > 16) {
            signal_error_and_exit(OPENCV_ERR);
        }

        /* Get bayer pattern from metadata or use parameter */
        char bayer_pattern[16];
        strcpy(bayer_pattern, "GRBG"); // Default

        unsigned char *input_image_data;
        size_t size = get_image_data(i, &input_image_data);

        /* Create OpenCV Mat from raw data */
        cv::Mat rawImage;
        
        if (bits_pixel <= 8) {
            rawImage = cv::Mat(height, width, CV_8UC1, input_image_data);
        } else {
            rawImage = cv::Mat(height, width, CV_16UC1, input_image_data);
            if (bits_pixel == 12) {
                /* Mask to 12-bit values */
                uint16_t* ptr = (uint16_t*)rawImage.data;
                size_t total_pixels = width * height;
                for (size_t j = 0; j < total_pixels; j++) {
                    ptr[j] = ptr[j] & 0x0FFF;
                }
            }
        }

        /* Convert to 8-bit for demosaicing */
        cv::Mat rawImage8bit;
        try {
            if (bits_pixel <= 8) {
                rawImage.copyTo(rawImage8bit);
            } else {
                rawImage.convertTo(rawImage8bit, CV_8U, 255.0/((1 << bits_pixel) - 1));
            }
            
            /* Apply vertical flip */
            cv::Mat processedImage;
            cv::flip(rawImage8bit, processedImage, 0);  // 0 means vertical flip

            /* Determine OpenCV color code for demosaicing */
            int cv_color_code;
            /* Using GRBG bayer pattern */
            cv_color_code = cv::COLOR_BayerGR2BGR;
            
            /* Perform demosaicing */
            cv::Mat demosaicedImage;
            cv::cvtColor(processedImage, demosaicedImage, cv_color_code);
        } catch (const cv::Exception& e) {
            free(input_image_data);
            signal_error_and_exit(OPENCV_ERR);
        }

        /* Get output data and size */
        size_t output_size = demosaicedImage.total() * demosaicedImage.elemSize();
        unsigned char *output_image_data = (unsigned char *)malloc(output_size);

        /* Check for malloc error */
        if (output_image_data == NULL)
        {
            signal_error_and_exit(MALLOC_ERR);
        }

        /* Copy OpenCV Mat data to output buffer */
        memcpy(output_image_data, demosaicedImage.data, output_size);

        /* Create image metadata for output */
        Metadata new_meta = METADATA__INIT;
        new_meta.size = output_size;
        new_meta.width = demosaicedImage.cols;
        new_meta.height = demosaicedImage.rows;
        new_meta.channels = demosaicedImage.channels();  // Should be 3 for BGR
        new_meta.timestamp = timestamp;
        new_meta.bits_pixel = 8;  // Output is 8-bit
        new_meta.camera = camera;

        /* Add custom metadata */
        add_custom_metadata_string(&new_meta, "demosaic_bayer_pattern", bayer_pattern);
        add_custom_metadata_string(&new_meta, "color_space", "BGR");

        /* Append the image to the result batch */
        append_result_image(output_image_data, output_size, &new_meta);

        /* Free allocated memory */
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
