#include "module.h"
#include "util.h"

#include <filesystem>
#include <tensorflow/lite/delegates/external/external_delegate.h>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>
#include <tensorflow/lite/optional_debug_tools.h>

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

    /* Retrieve module parameters by name (defined in config.yaml) */
    char *model_filename = get_param_string("model_filename");

    // Retrieve the class idx of interest
    // This is the class that will be kept, the other class will be set to all black for better compression
    int class_idx = get_param_int("class_index");

    float threshold_percentage = get_param_float("threshold_percentage");
    
    // Load the model
    std::unique_ptr<tflite::FlatBufferModel> model =
        tflite::FlatBufferModel::BuildFromFile(model_filename);

    // Define resolver
    tflite::ops::builtin::BuiltinOpResolver resolver;

    // Build the interpreter
    tflite::InterpreterBuilder builder(*model, resolver);
    std::unique_ptr<tflite::Interpreter> interpreter;
    if (builder(&interpreter) != kTfLiteOk)
    {
        signal_error_and_exit(INTERPRET_INIT);
    }
    if (interpreter == nullptr)
    {
        signal_error_and_exit(INTERPRET_INIT);
    }

    // Load the custom delegate
    auto ext_delegate_option =
        TfLiteExternalDelegateOptionsDefault("/usr/lib/libvx_delegate.so");

    // set the caching options
    const char *allow_cache_key = "allowed_cache_mode";
    const char *allow_cache_value = "true";
    const char *cache_file_key = "cache_file_path";
    // get the last part of the model filename to use in cache file name
    std::string cache_filename = std::string("cache_") + fs::path(model_filename).filename().string();

    const char *cache_file_value = cache_filename.c_str();
    ext_delegate_option.insert(&ext_delegate_option, allow_cache_key, allow_cache_value);
    ext_delegate_option.insert(&ext_delegate_option, cache_file_key, cache_file_value);
    ext_delegate_option.insert(&ext_delegate_option, "error_during_init", allow_cache_value);
    ext_delegate_option.insert(&ext_delegate_option, "error_during_prepare", allow_cache_value);
    ext_delegate_option.insert(&ext_delegate_option, "error_during_invoke", allow_cache_value);

    auto ext_delegate_ptr = TfLiteExternalDelegateCreate(&ext_delegate_option);

    // Modify the graph with delegate
    if (interpreter->ModifyGraphWithDelegate(ext_delegate_ptr) != kTfLiteOk)
    {
        signal_error_and_exit(INTERPRET_INIT);
    }

    // Allocate the tensors and get the input tensor
    if (interpreter->AllocateTensors() != kTfLiteOk)
    {
        signal_error_and_exit(TENSOR_ALLOC);
    }
    uint8_t *input_tensor = interpreter->typed_input_tensor<uint8_t>(0);

    // Get quantization parameters
    const auto *output_tensor = interpreter->output_tensor(0);
    const float scale = output_tensor->params.scale;
    const float zero_point = output_tensor->params.zero_point;

    for (int i = 0; i < num_images; ++i)
    {
        Metadata *input_meta = get_metadata(i);
        int height = input_meta->height;
        int width = input_meta->width;
        int channels = input_meta->channels;

        uint8_t *input_image_data;
        size_t size = get_image_data(i, &input_image_data);


        // memcpy the entire output image into input tensor

        memcpy(
            input_tensor,
            input_image_data,
            size);

        // infer and deal with the result
        if (interpreter->Invoke() != kTfLiteOk)
        {
            signal_error_and_exit(INFER_ERR);
        }

        uint8_t *scores = interpreter->typed_output_tensor<uint8_t>(0);

        int img_size = height * width * channels;
        int kept_pixels = img_size;
        for (int pix = 0; pix < height * width; pix++)
        {
            float scaled_score = static_cast<float>(scores[pix] - zero_point) * scale;
            bool keep = true;
            if (scaled_score < 0.5)
            {
                // set to class 0
                if (class_idx != 0)
                {
                    keep = false;
                }
            }
            else
            {
                if (class_idx != 1)
                {
                    keep = false;
                }
            }

            if (!keep)
            {
                // set pixel to black
                input_image_data[pix * channels + 0] = 0;
                input_image_data[pix * channels + 1] = 0;
                input_image_data[pix * channels + 2] = 0;
                kept_pixels -= channels;
            }
        }


        if ((float)kept_pixels > threshold_percentage * (float)img_size)
        {
            /* Append the image to the result batch */
            Metadata new_meta = METADATA__INIT;
            if (clone_metadata(input_meta, &new_meta) != 0)
            {
                signal_error_and_exit(MALLOC_ERR);
            }
            append_result_image(input_image_data, size, &new_meta);
        }

        // Free the input image

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
