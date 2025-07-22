#include "module.h"
#include "util.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DARK_THRESHOLD 10
#define MIN_DARK_COUNT 3
#define NUM_THREADS 4  // Adjust this depending on the number of cores

enum ERROR_CODE {
    MALLOC_ERR = 1,
    PLACEHOLDER = 2,
};

typedef struct {
    int start_index;
    int end_index;
    int num_pixels;
    uint16_t *local_accumulator;
    uint16_t width;
    uint16_t height;
} ThreadData;

static void *process_images_thread(void *arg)
{
    ThreadData *data = (ThreadData *)arg;

    for (int i = data->start_index; i < data->end_index; ++i)
    {
        Metadata *meta = get_metadata(i);
        if (!meta) continue;

        unsigned char *image_data;
        size_t size = get_image_data(i, &image_data);
        if (!image_data || size != data->num_pixels) {
            free(image_data);
            continue;
        }

        for (int j = 0; j < data->num_pixels; ++j)
        {
            if (image_data[j] < DARK_THRESHOLD)
                data->local_accumulator[j]++;
        }

        // Pass image unchanged to result batch
        Metadata new_meta = *meta;
        append_result_image(image_data, size, &new_meta);
        free(image_data);
    }

    return NULL;
}

/* START MODULE IMPLEMENTATION */
void module()
{
    int num_images = get_input_num_images();
    if (num_images == 0) return;

    Metadata *meta = get_metadata(0);
    if (!meta) return;

    uint16_t width = meta->width;
    uint16_t height = meta->height;
    int num_pixels = width * height;

    // Allocate global accumulator
    uint16_t *accumulator = calloc(num_pixels, sizeof(uint16_t));
    if (!accumulator) return;

    // Set up threads
    pthread_t threads[NUM_THREADS];
    ThreadData thread_data[NUM_THREADS];

    for (int t = 0; t < NUM_THREADS; ++t)
    {
        thread_data[t].start_index = (num_images * t) / NUM_THREADS;
        thread_data[t].end_index = (num_images * (t + 1)) / NUM_THREADS;
        thread_data[t].num_pixels = num_pixels;
        thread_data[t].width = width;
        thread_data[t].height = height;
        thread_data[t].local_accumulator = calloc(num_pixels, sizeof(uint16_t));
        if (!thread_data[t].local_accumulator) {
            for (int k = 0; k < t; ++k) free(thread_data[k].local_accumulator);
            free(accumulator);
            return;
        }

        pthread_create(&threads[t], NULL, process_images_thread, &thread_data[t]);
    }

    // Join threads and merge results
    for (int t = 0; t < NUM_THREADS; ++t)
    {
        pthread_join(threads[t], NULL);

        for (int j = 0; j < num_pixels; ++j)
            accumulator[j] += thread_data[t].local_accumulator[j];

        free(thread_data[t].local_accumulator);
    }

    // (Optional) Create and use the defect mask based on MIN_DARK_COUNT
    unsigned char *defect_mask = malloc(num_pixels);
    if (defect_mask) {
        for (int j = 0; j < num_pixels; ++j) {
            defect_mask[j] = (accumulator[j] >= MIN_DARK_COUNT) ? 1 : 0;
        }
        // Example: use defect_mask here or save it if needed
        free(defect_mask);
    }

    free(accumulator);
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
