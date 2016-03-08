
#include "evx1dec.h"
#include "config.h"
#include "macroblock.h"

namespace evx {

evx_status engine_decode_frame(bit_stream *input, const evx_frame &frame, evx_context *context, image *output);

evx1_decoder_impl::evx1_decoder_impl()
{
    initialized = false;

    clear_frame(&frame);
    clear_header(&header);
}

evx1_decoder_impl::~evx1_decoder_impl()
{
    if (EVX_SUCCESS != clear())
    {
        evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }
}

evx_status evx1_decoder_impl::clear()
{
    if (!initialized)
    {
        return EVX_SUCCESS;
    }

    clear_frame(&frame);
    clear_context(&context);

    initialized = false;

    return EVX_SUCCESS;
}

evx_status evx1_decoder_impl::initialize(bit_stream *input)
{
    if (initialized)
    {
        // The decoder cannot be re-initialized unless it is first cleared.
        return evx_post_error(EVX_ERROR_INVALID_RESOURCE);
    }

    // Read our header out of the stream.
    input->read_bytes(&header, sizeof(evx_header));

    if (evx_failed(verify_header(header)))
    {
        return evx_post_error(EVX_ERROR_INVALID_RESOURCE);
    }

    // Initialize image resources.
    uint32 aligned_width = align(header.frame_width, EVX_MACROBLOCK_SIZE);
    uint32 aligned_height = align(header.frame_height, EVX_MACROBLOCK_SIZE);

    if (EVX_SUCCESS != initialize_context(aligned_width, aligned_height, &context))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    initialized = true;

    return EVX_SUCCESS;
}

evx_status evx1_decoder_impl::read_frame_desc(bit_stream *input)
{
    evx_frame incoming_frame;

    input->read_bytes(&incoming_frame, sizeof(evx_frame));
    
    if (incoming_frame.index != frame.index)
    {
        return evx_post_error(EVX_ERROR_INVALID_RESOURCE);
    }

    frame = incoming_frame;

    return EVX_SUCCESS;
}

evx_status evx1_decoder_impl::decode(bit_stream *input, void *output)
{
    // Decode's job is to setup the pipeline and ensure that the incoming 
    // frame matches the expected dimensions.

    if (EVX_PARAM_CHECK)
    {
        if (!input || !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    if (!initialized)
    {
        if (evx_failed(initialize(input)))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    // Read our frame descriptor and configure our state for this frame.
    if (evx_failed(read_frame_desc(input)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (EVX_SUCCESS != decode_frame(input, output))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    frame.index++;
    input->empty();

    return EVX_SUCCESS;
}

evx_status evx1_decoder_impl::decode_frame(bit_stream *input, void *output)
{
    image output_image;

    if (evx_failed(create_image(EVX_IMAGE_FORMAT_R8G8B8, output, header.frame_width, header.frame_height, &output_image)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    return engine_decode_frame(input, frame, &context, &output_image);
}

} // namespace evx