#include "module.h"
#include "util.h"

#include <filesystem>
#include <tensorflow/lite/delegates/external/external_delegate.h>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>
#include <tensorflow/lite/optional_debug_tools.h>

#include <iostream>
#include <memory>
#include <vector>
#include <ctime> // time

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

    // Allocate the tensors
    if (interpreter->AllocateTensors() != kTfLiteOk)
    {
        signal_error_and_exit(TENSOR_ALLOC);
    }
    // Get quantization parameters

    const auto *output_tensor = interpreter->output_tensor(0);
    const float scale = output_tensor->params.scale;
    const float zero_point = output_tensor->params.zero_point;

    // Get output dimensions

    int output = interpreter->outputs()[0];
    TfLiteIntArray *output_dims = interpreter->tensor(output)->dims;
    // assume output dims to be something like (1, 1, ... ,size)
    auto output_size = output_dims->data[output_dims->size - 1];


    for (int i = 0; i < num_images; ++i)
    {
        Metadata *input_meta = get_metadata(i);
        // int height = input_meta->height;
        // int width = input_meta->width;
        // int channels = input_meta->channels;
        // int bits_pixel = input_meta->bits_pixel;

        uint8_t *input_image_data;
        size_t size = get_image_data(i, &input_image_data);


        // Get input tensor pointer and expected size each iteration (avoid stale pointer)
        int input_index = interpreter->inputs()[0];
        uint8_t *input_tensor = interpreter->typed_tensor<uint8_t>(input_index);

        TfLiteTensor *in_tensor = interpreter->tensor(input_index);
        size_t expected_bytes = in_tensor->bytes;

        // Copy image data to tensor
        memcpy(input_tensor, input_image_data, expected_bytes);


        // infer and deal with the result
        if (interpreter->Invoke() != kTfLiteOk)
        {
            signal_error_and_exit(INFER_ERR);
        }

        // uint8_t *scores = interpreter->typed_output_tensor<uint8_t>(0);
        float max_val = -1.0;
        int max_cls = -1;
        uint8_t *scores = interpreter->typed_output_tensor<uint8_t>(0);
        for (int j = 0; j < output_size; ++j)
        {
            float scaled_score = static_cast<float>(scores[j] - zero_point) * scale;
            if (scaled_score > max_val)
            {
                max_val = scaled_score;
                max_cls = j;
            }
        }


        // logger_log(logger, LOG_INFO, "Got the top class.");

        // send only the patches that match the class idx of interest
        if (max_cls == class_idx)
        {
            Metadata new_meta = METADATA__INIT;
            if (clone_metadata(input_meta, &new_meta) != 0)
            {
                signal_error_and_exit(MALLOC_ERR);
            }

            /* Add custom metadata key-value for prediction */
            add_custom_metadata_int(&new_meta, "prediction", max_cls);

            /* Append the image to the result batch */
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
