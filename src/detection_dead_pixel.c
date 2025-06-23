#include "module.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

enum ERROR_CODE {
    MALLOC_ERR = 1,
    FILE_IO_ERR = 2,
    DIMENSION_MISMATCH_ERR = 3,
};

#define MASK_PATH "defect_mask.bin"
#define DARK_THRESHOLD 10           // Pixel value below this is considered "dark"
#define MIN_DARK_COUNT 3            // Pixel must be dark in at least this many images to be flagged

static unsigned char *current_mask = NULL;
static uint16_t current_width = 0;
static uint16_t current_height = 0;

// Compare two masks to see if they differ
static bool mask_changed(unsigned char *a, unsigned char *b, size_t size) {
    return memcmp(a, b, size) != 0;
}

// Save mask to file in binary format (width, height, then data)
static bool save_mask_to_file(uint16_t width, uint16_t height, unsigned char *mask) {
    FILE *f = fopen(MASK_PATH, "wb");
    if (!f) return false;
    fwrite(&width, sizeof(uint16_t), 1, f);           // write width (2 bytes)
    fwrite(&height, sizeof(uint16_t), 1, f);          // write height (2 bytes)
    fwrite(mask, 1, width * height, f);               // write mask data
    fclose(f);
    return true;
}

void module() {
    printf("Calling get_input_num_images()\n"); //debugg
    int num_images = get_input_num_images();
    printf("Number of images: %d\n", num_images); //debug
    if (num_images == 0) return;

    // Assume all images have same dimensions
    Metadata *meta = get_metadata(0);
    if (!meta) { //debugg
        printf("Error: metadata for first image is NULL.\n");
        return;
    }
    uint16_t w = meta->width;
    uint16_t h = meta->height;      
    size_t pixels = w * h;

    // Allocate an accumulator to count how many times each pixel is dark
    uint16_t *accumulator = calloc(pixels, sizeof(uint16_t));
    if (!accumulator) {
        printf("Error (MALLOC_ERR): Failed to allocate accumulator.\n");
        return;
    }

    // Iterate through each image and count "dark" pixels
    for (int i = 0; i < num_images; ++i) {
        Metadata *input_meta = get_metadata(i);
        if (!input_meta) { //debugg
            printf("Error: input_meta is NULL for image %d\n", i);
            free(accumulator);
            return;
            }

        unsigned char *image_data;
        printf("Processing image %d\n", i); //debugg
        size_t size = get_image_data(i, &image_data);
        if (!image_data || size != pixels) { //debugg
            printf("Error: image_data is NULL or unexpected size (got %zu, expected %zu).\n", size, pixels);
            free(accumulator);
            return;
            }

        // Sanity check dimensions match
        if (input_meta->width != w || input_meta->height != h) {
            printf("Error (DIMENSION_MISMATCH_ERR): Image dimensions mismatch in batch.\n");
            free(image_data);
            free(accumulator);
            return;
        }

        // For each pixel: increment count if pixel is dark
        for (size_t j = 0; j < pixels; ++j) {
            if (image_data[j] < DARK_THRESHOLD) {
                accumulator[j]++;
            }
        }

        // Pass image unchanged to result
        Metadata new_meta = *input_meta;
        append_result_image(image_data, size, &new_meta);
        free(image_data);
    }

    // Build new defect mask based on dark pixel counts
    unsigned char *new_mask = malloc(pixels);
    if (!new_mask) {
        printf("Error (MALLOC_ERR): Failed to allocate new defect mask.\n");
        free(accumulator);
        return;
    }

    for (size_t j = 0; j < pixels; ++j) {
        new_mask[j] = (accumulator[j] >= MIN_DARK_COUNT) ? 1 : 0;
    }

    free(accumulator);

    // Save new mask if it's different or first run
    if (!current_mask || w != current_width || h != current_height ||
        mask_changed(current_mask, new_mask, pixels)) {

        if (!save_mask_to_file(w, h, new_mask)) {
            printf("Error (FILE_IO_ERR): Failed to save defect mask.\n");
            free(new_mask);
            return;
        }

        free(current_mask);
        current_mask = new_mask;
        current_width = w;
        current_height = h;
    } else {
        free(new_mask);  // No update needed
    }
}

// Standard run wrapper
ImageBatch run(ImageBatch *input_batch, ModuleParameterList *module_parameter_list, int *ipc_error_pipe) {
    printf("I am running");
    ImageBatch result_batch;
    result = &result_batch;
    input = input_batch;
    config = module_parameter_list;
    error_pipe = ipc_error_pipe;
    initialize();
    printf("We are initialized.");
    module();

    finalize();
    return result_batch;
}
