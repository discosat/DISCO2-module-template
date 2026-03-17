#include "module.h"
#include "util.h"
#include <stdbool.h>
#include <openjpeg.h>
#include <string.h>

/* Define custom error codes */
enum ERROR_CODE {
    MALLOC_ERR = 1,
    INVALD_QUALITY_ERR = 2,
    OPJ_IMAGE_CREATE_ERR = 3,
    OPJ_COMPRESS_ERR = 4,
    OPJ_CODEC_CREATE_ERR = 5,
    OPJ_SETUP_ERR = 6,
    OPJ_STREAM_ERR = 7,
    OPJ_ENCODE_ERR = 8,
};

/* Memory stream for openjpeg buffer output */
typedef struct {
    unsigned char *data;
    size_t size;
    size_t capacity;
    size_t offset;
} mem_stream_t;

static OPJ_SIZE_T mem_stream_write(void *p_buffer, OPJ_SIZE_T p_nb_bytes, void *p_user_data)
{
    mem_stream_t *s = (mem_stream_t *)p_user_data;
    size_t needed = s->offset + p_nb_bytes;
    if (needed > s->capacity) {
        size_t new_cap = s->capacity * 2;
        if (new_cap < needed)
            new_cap = needed;
        unsigned char *tmp = realloc(s->data, new_cap);
        if (!tmp)
            return (OPJ_SIZE_T)-1;
        s->data = tmp;
        s->capacity = new_cap;
    }
    memcpy(s->data + s->offset, p_buffer, p_nb_bytes);
    s->offset += p_nb_bytes;
    if (s->offset > s->size)
        s->size = s->offset;
    return p_nb_bytes;
}

static OPJ_BOOL mem_stream_seek(OPJ_OFF_T p_nb_bytes, void *p_user_data)
{
    mem_stream_t *s = (mem_stream_t *)p_user_data;
    if (p_nb_bytes < 0)
        return OPJ_FALSE;
    s->offset = (size_t)p_nb_bytes;
    return OPJ_TRUE;
}

static OPJ_OFF_T mem_stream_skip(OPJ_OFF_T p_nb_bytes, void *p_user_data)
{
    mem_stream_t *s = (mem_stream_t *)p_user_data;
    if (p_nb_bytes < 0)
        return -1;
    s->offset += (size_t)p_nb_bytes;
    return p_nb_bytes;
}

static void mem_stream_free(void *p_user_data)
{
    /* Do not free — caller owns the buffer */
    (void)p_user_data;
}

int compress_rgb_to_jp2(const unsigned char* input_data, int width, int height,
                        int channels, int bits_pixel, void** out_buffer,
                        size_t* out_len, bool islossless, int quality)
{
    int bytes_per_sample = (bits_pixel > 8) ? 2 : 1;

    /* Set up component parameters */
    opj_image_cmptparm_t *cmptparm = calloc(channels, sizeof(opj_image_cmptparm_t));
    if (!cmptparm)
        return 0;

    for (int c = 0; c < channels; c++) {
        cmptparm[c].dx = 1;
        cmptparm[c].dy = 1;
        cmptparm[c].w = (OPJ_UINT32)width;
        cmptparm[c].h = (OPJ_UINT32)height;
        cmptparm[c].prec = (OPJ_UINT32)bits_pixel;
        cmptparm[c].sgnd = 0;
    }

    OPJ_COLOR_SPACE color_space;
    if (channels >= 3)
        color_space = OPJ_CLRSPC_SRGB;
    else
        color_space = OPJ_CLRSPC_GRAY;

    opj_image_t *image = opj_image_create(channels, cmptparm, color_space);
    free(cmptparm);
    if (!image)
        return 0;

    image->x0 = 0;
    image->y0 = 0;
    image->x1 = (OPJ_UINT32)width;
    image->y1 = (OPJ_UINT32)height;

    /* Copy interleaved pixel data into planar component arrays */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixel_idx = y * width + x;
            for (int c = 0; c < channels; c++) {
                int sample_offset = (pixel_idx * channels + c) * bytes_per_sample;
                OPJ_INT32 val;
                if (bytes_per_sample == 2) {
                    val = (OPJ_INT32)(input_data[sample_offset]
                                      | (input_data[sample_offset + 1] << 8));
                } else {
                    val = (OPJ_INT32)input_data[sample_offset];
                }
                image->comps[c].data[pixel_idx] = val;
            }
        }
    }

    /* Set up compression parameters */
    opj_cparameters_t parameters;
    opj_set_default_encoder_parameters(&parameters);
    parameters.cod_format = 0; /* J2K codestream */
    parameters.tcp_numlayers = 1;
    parameters.cp_disto_alloc = 1;

    if (islossless) {
        parameters.irreversible = 0; /* 5-3 reversible wavelet */
        parameters.tcp_rates[0] = 0; /* lossless */
    } else {
        parameters.irreversible = 1; /* 9-7 irreversible wavelet */
        /* quality 1-100 maps to compression ratio: quality 100 = rate 1 (best),
           quality 1 = rate 100 (most compressed) */
        parameters.tcp_rates[0] = (float)(101 - quality);
    }

    opj_codec_t *codec = opj_create_compress(OPJ_CODEC_J2K);
    if (!codec) {
        opj_image_destroy(image);
        return 0;
    }

    if (!opj_setup_encoder(codec, &parameters, image)) {
        opj_destroy_codec(codec);
        opj_image_destroy(image);
        return 0;
    }

    /* Create memory-backed output stream */
    size_t initial_cap = (size_t)width * height * channels * bytes_per_sample;
    mem_stream_t mstream = {
        .data = malloc(initial_cap),
        .size = 0,
        .capacity = initial_cap,
        .offset = 0,
    };
    if (!mstream.data) {
        opj_destroy_codec(codec);
        opj_image_destroy(image);
        return 0;
    }

    opj_stream_t *stream = opj_stream_create(OPJ_J2K_STREAM_CHUNK_SIZE, OPJ_FALSE);
    if (!stream) {
        free(mstream.data);
        opj_destroy_codec(codec);
        opj_image_destroy(image);
        return 0;
    }

    opj_stream_set_user_data(stream, &mstream, mem_stream_free);
    opj_stream_set_write_function(stream, mem_stream_write);
    opj_stream_set_seek_function(stream, mem_stream_seek);
    opj_stream_set_skip_function(stream, mem_stream_skip);

    OPJ_BOOL ok = opj_start_compress(codec, image, stream);
    if (ok)
        ok = opj_encode(codec, stream);
    if (ok)
        ok = opj_end_compress(codec, stream);

    opj_stream_destroy(stream);
    opj_destroy_codec(codec);
    opj_image_destroy(image);

    if (!ok) {
        free(mstream.data);
        return 0;
    }

    *out_buffer = mstream.data;
    *out_len = mstream.size;
    return 1;
}

/* START MODULE IMPLEMENTATION */
void module()
{
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
        int bits_pixel = input_meta->bits_pixel;

        unsigned char *image_data;
        get_image_data(i, &image_data);

        void *compressed_data = NULL;
        size_t compressed_size = 0;

        int success = compress_rgb_to_jp2(image_data, width, height, channels,
                                          bits_pixel, &compressed_data,
                                          &compressed_size, lossless, quality);

        if(!success || !compressed_data){
            free(image_data);
            signal_error_and_exit(OPJ_COMPRESS_ERR);
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
        free(compressed_data);
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
