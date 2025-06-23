#include "module.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//this one is wroooooong

enum ERROR_CODE {
    MALLOC_ERR = 1,
    PLACEHOLDER = 2,
};

#define MAX_WIDTH 2464
#define MAX_HEIGHT 2056

/* Detect defective pixels by thresholding difference from local mean */
/* Outputs defect_mask: 1 = defective, 0 = good */
static void detect_defects(const unsigned char *image, int width, int height, unsigned char *defect_mask, int threshold)
{
    printf("[DEBUG] Starting defect detection with threshold %d\n", threshold);

    // Clear mask
    memset(defect_mask, 0, width * height);

    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            int sum = 0;
            int count = 0;
            // Compute 3x3 neighborhood mean excluding center pixel
            for (int ny = y - 1; ny <= y + 1; ny++)
            {
                for (int nx = x - 1; nx <= x + 1; nx++)
                {
                    if (nx == x && ny == y)
                        continue; // skip center pixel

                    sum += image[ny * width + nx];
                    count++;
                }
            }
            int mean = sum / count;
            int pixel_val = image[y * width + x];
            int diff = pixel_val - mean;
            if (diff < 0) diff = -diff;

            // Mark pixel defective if difference above threshold OR stuck at 0 or 255
            if (pixel_val == 0 || pixel_val == 255)
            {
                defect_mask[y * width + x] = 1;
                // Uncomment for per-pixel debug:
                // printf("[DEBUG] Defective pixel detected at (%d,%d): val=%d, mean=%d, diff=%d\n", x, y, pixel_val, mean, diff);
            }
        }
    }

    printf("[DEBUG] Defect detection complete.\n");
}

/* Interpolate defective pixels by replacing them with the mean of valid neighbors */
static void interpolate_defects(unsigned char *image, int width, int height, const unsigned char *defect_mask)
{
    printf("[DEBUG] Starting defect pixel interpolation\n");

    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            if (defect_mask[y * width + x] == 1)
            {
                int sum = 0;
                int count = 0;
                // average neighbors that are NOT defective
                for (int ny = y - 1; ny <= y + 1; ny++)
                {
                    for (int nx = x - 1; nx <= x + 1; nx++)
                    {
                        if (nx == x && ny == y)
                            continue; // skip center pixel
                        if (defect_mask[ny * width + nx] == 0)
                        {
                            sum += image[ny * width + nx];
                            count++;
                        }
                    }
                }
                if (count > 0)
                {
                    image[y * width + x] = (unsigned char)(sum / count);
                    // printf("[DEBUG] Pixel (%d,%d) interpolated with value %d\n", x, y, image[y * width + x]);
                }
                else
                {
                    // No valid neighbors found (rare), leave pixel as is
                    // printf("[DEBUG] Pixel (%d,%d) no valid neighbors for interpolation\n", x, y);
                }
            }
        }
    }

    printf("[DEBUG] Interpolation complete.\n");
}

/* START MODULE IMPLEMENTATION */
void module()
{
    int num_images = get_input_num_images();

    if (num_images < 2) {
        printf("[ERROR] Need at least 2 images to detect stuck pixels\n");
        return;
    }

    /* Parameters */
    int width = MAX_WIDTH;
    int height = MAX_HEIGHT;
    int threshold = 10;

    unsigned char **all_images = malloc(sizeof(unsigned char *) * num_images);
    if (!all_images) {
        printf("[ERROR] Failed to allocate image buffer array\n");
        return;
    }

    Metadata *ref_meta = get_metadata(0);
    width = ref_meta->width;
    height = ref_meta->height;

    size_t image_size = width * height;

    // Load all images into memory
    for (int i = 0; i < num_images; ++i) {
        unsigned char *data;
        size_t size = get_image_data(i, &data);
        all_images[i] = data; // we free it later
    }

    // Allocate dead pixel map
    unsigned char *dead_pixel_mask = calloc(width * height, 1);
    if (!dead_pixel_mask) {
        printf("[ERROR] Failed to allocate dead pixel mask\n");
        return;
    }

    // Detect dead pixels across all frames
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            unsigned char first_val = all_images[0][idx];
            int is_stuck = 1;
            for (int i = 1; i < num_images; ++i) {
                if (all_images[i][idx] != first_val) {
                    is_stuck = 0;
                    break;
                }
            }
            if (is_stuck && (first_val == 0 || first_val == 255)) {
                dead_pixel_mask[idx] = 1;
            }
        }
    }

    printf("[DEBUG] Dead pixel mask built from %d images.\n", num_images);

    // Now fix each image
    for (int i = 0; i < num_images; ++i)
    {
        Metadata *input_meta = get_metadata(i);
        unsigned char *image_data = all_images[i]; // already loaded

        if (input_meta->channels != 1 || input_meta->bits_pixel != 8) {
            printf("[ERROR] Unsupported image format\n");
            append_result_image(image_data, image_size, input_meta);
            free(image_data);
            continue;
        }

        interpolate_defects(image_data, width, height, dead_pixel_mask);

        Metadata new_meta = METADATA__INIT;
        new_meta.size = image_size;
        new_meta.width = width;
        new_meta.height = height;
        new_meta.channels = input_meta->channels;
        new_meta.timestamp = input_meta->timestamp;
        new_meta.bits_pixel = input_meta->bits_pixel;
        new_meta.camera = input_meta->camera;

        append_result_image(image_data, image_size, &new_meta);

        free(image_data); // free original image data
    }

    free(dead_pixel_mask);
    free(all_images);

    printf("[DEBUG] All images processed with dead pixel correction.\n");
}

/* END MODULE IMPLEMENTATION */

void process_image(unsigned char *image_data, int width, int height, int channels, int bits_pixel)
{
    int threshold = 10; // same default

    if (channels != 1 || bits_pixel != 8)
    {
        printf("[ERROR] Unsupported image format: channels=%d, bits_pixel=%d\n", channels, bits_pixel);
        return;
    }

    unsigned char *defect_mask = malloc(width * height);
    if (!defect_mask)
    {
        printf("[ERROR] Failed to allocate defect mask\n");
        return;
    }

    detect_defects(image_data, width, height, defect_mask, threshold);
    interpolate_defects(image_data, width, height, defect_mask);

    free(defect_mask);
}


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

#ifdef TESTING_MODULE_STANDALONE

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        printf("Usage: %s <raw_image_file> <width> <height> <output_file>\n", argv[0]);
        return 1;
    }

    const char *input_filename = argv[1];
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);
    const char *output_filename = argv[4];

    size_t image_size = width * height;

    FILE *f = fopen(input_filename, "rb");
    if (!f)
    {
        perror("Error opening input file");
        return 1;
    }

    unsigned char *image_data = malloc(image_size);
    if (!image_data)
    {
        printf("Failed to allocate memory for image\n");
        fclose(f);
        return 1;
    }

    size_t read = fread(image_data, 1, image_size, f);
    fclose(f);

    if (read != image_size)
    {
        printf("Error: read %zu bytes, expected %zu\n", read, image_size);
        free(image_data);
        return 1;
    }

    // Process the image
    process_image(image_data, width, height, 1 /*channels*/, 8 /*bits_pixel*/);

    // Write the output image
    f = fopen(output_filename, "wb");
    if (!f)
    {
        perror("Error opening output file");
        free(image_data);
        return 1;
    }

    fwrite(image_data, 1, image_size, f);
    fclose(f);

    free(image_data);

    printf("Processed image saved to %s\n", output_filename);
    return 0;
}

#endif
