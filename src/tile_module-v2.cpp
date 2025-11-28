#include "module.h"
#include "util.h"

#include <filesystem>

namespace fs = std::filesystem;
/* Define custom error codes */
enum ERROR_CODE
{
    MALLOC_ERR = 1,
    INTERPRET_INIT = 2,
    TENSOR_ALLOC = 3,
    INFER_ERR = 4,
};

/* START MODULE IMPLEMENTATION */
void module()
{

    /* Get number of images in input batch */
    int num_images = get_input_num_images();

    // Retrieve tile size
    int tile_height = get_param_int("tile_height");
    int tile_width = get_param_int("tile_width");

    for (int i = 0; i < num_images; ++i)
    {
        Metadata *input_meta = get_metadata(i);
        int height = input_meta->height;
        int width = input_meta->width;
        int channels = input_meta->channels;
        int bits_pixel = input_meta->bits_pixel;

        uint8_t *input_image_data;
        size_t size = get_image_data(i, &input_image_data);

        // Determine bytes per pixel based on bit depth
        int bytes_per_sample = (bits_pixel <= 8) ? 1 : 2;
        size_t num_pixels = height * width * channels;

        // Allocate 8-bit output buffer for tiles
        uint8_t *parsed_data = (uint8_t *)malloc(num_pixels);
        if (parsed_data == NULL)
        {
            signal_error_and_exit(MALLOC_ERR);
        }

        if (bits_pixel <= 8)
        {
            // 8-bit data in 8-bit container - direct copy
            memcpy(parsed_data, input_image_data, num_pixels);
        }
        else
        {
            // 16-bit data in 16-bit container - scale down to 8-bit
            int shift = bits_pixel - 8;
            uint16_t *input_16 = (uint16_t *)input_image_data;
            for (size_t pix = 0; pix < num_pixels; pix++)
            {
                parsed_data[pix] = (uint8_t)(input_16[pix] >> shift);
            }
        }

        int tile_bytes = tile_height * tile_width * channels * sizeof(uint8_t);
        int tile_idx = 0;

        // Define the arrays that store patches to be passed to next modules
        uint8_t *output_image_data = (uint8_t *)malloc(tile_bytes);

        /* Check for malloc error */
        if (output_image_data == NULL)
        {
            free(parsed_data);
            signal_error_and_exit(MALLOC_ERR);
        }

        for (uint16_t height_offset = 0; height_offset + tile_height <= height; height_offset += tile_height)
        {
            for (uint16_t width_offset = 0; width_offset + tile_width <= width; width_offset += tile_width)
            {

                for (uint16_t h = height_offset; h < tile_height + height_offset; h++)
                {
                    // memcpy row of a patch into the output data
                    memcpy(
                        output_image_data + ((h - height_offset) * tile_width * channels),
                        parsed_data + (h * width * channels + width_offset * channels),
                        sizeof(uint8_t) * tile_width * channels);
                }

                /* Create image metadata before appending */
                Metadata new_meta = METADATA__INIT;
                if (clone_metadata(input_meta, &new_meta) != 0)
                {
                    free(output_image_data);
                    free(parsed_data);
                    signal_error_and_exit(MALLOC_ERR);
                }
                new_meta.size = tile_bytes;
                new_meta.width = tile_width;
                new_meta.height = tile_height;
                new_meta.bits_pixel = 8; // Output is always 8-bit

                // add tile_idx to metadata
                add_custom_metadata_int(&new_meta, "tile_idx", tile_idx);

                /* Append the image to the result batch */
                append_result_image(output_image_data, tile_bytes, &new_meta);

                tile_idx++;
            }
        }

        // Free the tile memory
        free(output_image_data);
        // Free the input image
        free(input_image_data);
        free(parsed_data);
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