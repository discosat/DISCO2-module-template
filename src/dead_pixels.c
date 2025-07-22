#include "module.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DARK_THRESHOLD 10
#define MIN_DARK_COUNT 3

/* Define custom error codes */
enum ERROR_CODE {
    MALLOC_ERR = 1,
    PLACEHOLDER = 2,
};

/* START MODULE IMPLEMENTATION */
void module()
{
    int num_images = get_input_num_images();
    printf("[DEBUG] Number of images to process: %d\n", num_images);
    if (num_images == 0) return;

    Metadata *meta = get_metadata(0);
    if (!meta) {
        printf("[ERROR] Failed to get metadata for first image\n");
        return;
    }

    uint16_t width = meta->width;
    uint16_t height = meta->height;
    size_t num_pixels = (size_t)width * height * 2; //2 is the numnber of channels

    // Allocate accumulator to count how many times each pixel is dark
    uint16_t *accumulator = calloc(num_pixels, sizeof(uint16_t));
    if (!accumulator) {
        printf("[ERROR] Failed to allocate accumulator\n");
        return;
    }

    // First pass: count dark pixels across all images
    for (int i = 0; i < num_images; ++i) {
        Metadata *input_meta = get_metadata(i);
        if (!input_meta) {
            printf("[ERROR] Metadata NULL for image %d\n", i);
            free(accumulator);
            return;
        }

        unsigned char *image_data;
        size_t size = get_image_data(i, &image_data);
        if (!image_data || size != num_pixels) {
            printf("[ERROR] Invalid image data for image %d\n", i); //this prints
            free(accumulator);
            return;
        }

        for (size_t p = 0; p < num_pixels; ++p) {
            if (image_data[p] < DARK_THRESHOLD) {
                accumulator[p]++;
            }
        }
        free(image_data);
    }

    // Build dead pixel mask based on threshold
    unsigned char *dead_pixel_mask = malloc(num_pixels);
    if (!dead_pixel_mask) {
        printf("[ERROR] Failed to allocate dead pixel mask\n");
        free(accumulator);
        return;
    }

    int total_dead_pixels = 0;
    for (size_t p = 0; p < num_pixels; ++p) {
        dead_pixel_mask[p] = (accumulator[p] >= MIN_DARK_COUNT) ? 1 : 0;
        if (dead_pixel_mask[p]) total_dead_pixels++;
    }
    printf("[DEBUG] Total dead pixels detected: %d\n", total_dead_pixels);

    // Save dead pixel mask to disk for visualization (raw 8-bit grayscale)
    FILE *mask_file = fopen("/tmp/dead_pixel_mask.raw", "wb");
    if (mask_file) {
        fwrite(dead_pixel_mask, 1, num_pixels, mask_file);
        fclose(mask_file);
        printf("[DEBUG] Dead pixel mask saved to /tmp/dead_pixel_mask.raw\n");
    } else {
        printf("[ERROR] Failed to save dead pixel mask to file\n");
    }

    // Second pass: process images again, removing dead pixels by setting to 255
    for (int i = 0; i < num_images; ++i) {
        Metadata *input_meta = get_metadata(i);
        if (!input_meta) {
            printf("[ERROR] Metadata NULL for image %d\n", i);
            free(accumulator);
            free(dead_pixel_mask);
            return;
        }

        unsigned char *image_data;
        size_t size = get_image_data(i, &image_data);
        if (!image_data || size != num_pixels) {
            printf("[ERROR] Invalid image data for image %d\n", i);
            free(accumulator);
            free(dead_pixel_mask);
            return;
        }

        // Remove dead pixels: set to max value (white)
        int dead_pixels_removed = 0;
        for (size_t p = 0; p < num_pixels; ++p) {
            if (dead_pixel_mask[p]) {
                image_data[p] = 255;
                dead_pixels_removed++;
            }
        }

        printf("[DEBUG] Image %d: removed %d dead pixels\n", i, dead_pixels_removed);

        // Prepare new metadata and append processed image to result
        Metadata new_meta = METADATA__INIT;
        new_meta.size = input_meta->size;
        new_meta.width = input_meta->width;
        new_meta.height = input_meta->height;
        new_meta.channels = input_meta->channels;
        new_meta.timestamp = input_meta->timestamp;
        new_meta.bits_pixel = input_meta->bits_pixel;
        new_meta.camera = input_meta->camera;
        new_meta.obid = input_meta->obid;

        append_result_image(image_data, size, &new_meta);

        free(image_data);
    }

    free(accumulator);
    free(dead_pixel_mask);
}
/* END MODULE IMPLEMENTATION */

/* Main function of module (NO NEED TO MODIFY) */
ImageBatch run(ImageBatch *input_batch, ModuleParameterList *module_parameter_list, int *ipc_error_pipe)
{
    printf("I am here hehehe\n");
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
