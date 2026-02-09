#include "module.h"
#include "util.h"

/* Define custom error codes */
enum ERROR_CODE {
    MALLOC_ERR = 1,
    PLACEHOLDER = 2,
};

/* START MODULE IMPLEMENTATION */
void module()
{
    /* Get number of images in input batch */
    int num_images = get_input_num_images();

    /* Example code for iterating a pixel value at a time */
    for (int i = 0; i < num_images; ++i)
    {
        
        unsigned char *input_image_data;
        size_t size = get_image_data(i, &input_image_data);

        /* Get input image metadata */
        Metadata *input_meta = get_metadata(i);
        int height = input_meta->height;
        int width = input_meta->width;
        int channels = input_meta->channels;
        int timestamp = input_meta->timestamp;
        int bits_pixel = input_meta->bits_pixel;
        char *camera = input_meta->camera;
        int obid = input_meta->obid;
        int exposure = get_custom_metadata_int(input_meta, "exposure");
        float iso = get_custom_metadata_float(input_meta, "iso");
        int pipeline_id = get_custom_metadata_int(input_meta, "pipeline_id");

        Metadata new_meta = METADATA__INIT;
            if (clone_metadata(input_meta, &new_meta) != 0)
            {
                signal_error_and_exit(MALLOC_ERR);
            }

        /* Append the image to the result batch */
        append_result_image(input_image_data, size, &new_meta);
        
        /* Remember to free any allocated memory */
        free(input_image_data);
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