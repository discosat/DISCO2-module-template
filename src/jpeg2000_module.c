#include "module.h"
#include "util.h"
#include <stdbool.h>
#include <openjpeg-2.5/openjpeg.h>
#include <string.h>
#include <vips/vips.h>
#include <vips/image.h>

/* Define custom error codes */
enum ERROR_CODE {
    MALLOC_ERR = 1,
    INVALD_QUALITY_ERR = 2,
    VIPS_INIT_ERR = 3,
    VIPS_COMPRESS_ERR = 4,
};

int compress_rgb_to_jp2(const unsigned char* input_data, int width, int height, int channels, int bits_pixel, void** out_buffer, size_t* out_len, bool islossless, int quality) {
    VipsBandFormat format;
    size_t data_size;

    if(bits_pixel == 8){
        format = VIPS_FORMAT_UCHAR;
        data_size = (size_t)width * height * channels;
    } else {
        format = VIPS_FORMAT_USHORT;
        data_size = (size_t)width * height * channels * 2;
    }
    
    VipsImage* image = vips_image_new_from_memory(input_data, data_size, width, height, channels, format);
    
    int result;
    if(islossless){
        result = vips_jp2ksave_buffer(image, out_buffer, out_len, "lossless", TRUE, "Q", quality, NULL);
    } else {
        result = vips_jp2ksave_buffer(image, out_buffer, out_len, "Q", quality, NULL);
    }
 
    g_object_unref(image);
 
    return (result == 0) ? 1 : 0;
}

/* START MODULE IMPLEMENTATION */
void module()
{
    if (VIPS_INIT("jp2k-compression") != 0) {
        signal_error_and_exit(VIPS_INIT_ERR);
    }

    /* Get number of images in input batch */
    int num_images = get_input_num_images();

    bool lossless = get_param_bool("lossless");
    int quality = get_param_int("quality"); 

    if(quality <= 0 || quality > 100){
        signal_error_and_exit(INVALD_QUALITY_ERR);
    }


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
      
        unsigned char *image_data;
        size_t size = get_image_data(i, &image_data);

        void *compressed_data = NULL;
        size_t compressed_size = 0;

        int success = compress_rgb_to_jp2(image_data, width, height, channels, bits_pixel, &compressed_data, &compressed_size, lossless, quality);

        if(!success || !compressed_data){
            free(image_data);
            signal_error_and_exit(VIPS_COMPRESS_ERR);
        }

        /* Create image metadata before appending */
        Metadata new_meta = METADATA__INIT;
            if (clone_metadata(input_meta, &new_meta) != 0)
            {
                signal_error_and_exit(MALLOC_ERR);
            }     
        
        new_meta.size = compressed_size;
        add_custom_metadata_string(&new_meta, "enc", "j2k");

        /* Append the image to the result batch */
        append_result_image(compressed_data, compressed_size, &new_meta);

        /* Remember to free any allocated memory */
        free(image_data);
        g_free(compressed_data);

    }

    vips_shutdown();
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