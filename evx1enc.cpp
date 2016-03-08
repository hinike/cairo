
#include "evx1enc.h"
#include "config.h"
#include "convert.h"
#include "macroblock.h"

namespace evx {

evx_status engine_encode_frame(const image &input, const evx_frame &frame_desc, evx_context *context, bit_stream *output);

evx1_encoder_impl::evx1_encoder_impl()
{
    initialized = false;

    clear_frame(&frame);
    clear_header(&header);
}

evx1_encoder_impl::~evx1_encoder_impl()
{
    if (EVX_SUCCESS != clear())
    {
        evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }
}

evx_status evx1_encoder_impl::clear()
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

evx_status evx1_encoder_impl::insert_intra()
{
    // This allows the host to determine when to issue a new i-frame. Usually this 
    // is due to a dropped packet event which causes the encoder/decoder to lose
    // sync and introduce a flush.

    frame.type = EVX_FRAME_INTRA;

    return EVX_SUCCESS;
}

evx_status evx1_encoder_impl::set_quality(uint8 quality)
{
    quality = clip_range(quality, 1, 31);

    // Quality can range from 1-31. Level 0 is the highest and should be 
    // set for efficiency research. For latency measurements, a level of
    // between 16 and 24 is usually preferred.

    frame.quality = quality;

    return EVX_SUCCESS;
}

evx_status evx1_encoder_impl::initialize(uint32 width, uint32 height)
{
    if (initialized)
    {
        // The encoder cannot be re-initialized unless it is first cleared.
        return evx_post_error(EVX_ERROR_INVALID_RESOURCE);
    } 

    // Initialize will place the encoder in a default state that is 
    // ready for encoding operations.
    initialize_header(width, height, &header);

    // Initialize image resources.
    uint32 aligned_width = align(width, EVX_MACROBLOCK_SIZE);
    uint32 aligned_height = align(height, EVX_MACROBLOCK_SIZE);

    if (EVX_SUCCESS != initialize_context(aligned_width, aligned_height, &context))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    initialized = true;

    return EVX_SUCCESS;
}

evx_status evx1_encoder_impl::encode(void *input, uint32 width, uint32 height, bit_stream *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!output || 0 == width || 0 == height || !input)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    // Encode's job is merely to setup the pipeline and ensure that the incoming 
    // frame matches the expected dimensions.

    if (!initialized)
    {
        if (evx_failed(initialize(width, height)))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }

        // serialize out our header to begin the stream.
        if (evx_failed(output->write_bytes(&header, sizeof(evx_header))))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    if (width != header.frame_width || height != header.frame_height)
    {
        // The incoming frame size does not match our current encoder settings.
        // We could adapt or simply rip. 

        return evx_post_error(EVX_ERROR_INVALID_RESOURCE);
    }

    // Serialize our frame state.
    if (evx_failed(output->write_bytes(&frame, sizeof(evx_frame))))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (evx_failed(encode_frame(input, width, height, output)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

#if EVX_ALLOW_INTER_FRAMES
    frame.type = EVX_FRAME_INTER;
#endif

    // Periodically insert intra frames into the stream. This enables seekability
    // and periodic error correction.

    if (EVX_PERIODIC_INTRA_RATE)
    {
        if (0 == ((frame.index + 1) % EVX_PERIODIC_INTRA_RATE))
        {
            insert_intra();
        }
    }

    frame.index++;

    return EVX_SUCCESS;
}

evx_status evx1_encoder_impl::encode_frame(void *input, uint32 width, uint32 height, bit_stream *output)
{
    image input_image;

    if (evx_failed(create_image(EVX_IMAGE_FORMAT_R8G8B8, input, width, height, &input_image)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    return engine_encode_frame(input_image, frame, &context, output);
}

evx_status evx1_encoder_impl::peek(EVX_PEEK_STATE peek_state, void *output) 
{
    if (EVX_PARAM_CHECK)
    {
        if (!output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    if (!initialized)
    {
        return EVX_SUCCESS;
    }

    image output_image;
    
    if (evx_failed(create_image(EVX_IMAGE_FORMAT_R8G8B8, output, header.frame_width, header.frame_height, &output_image)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    switch (peek_state)
    {
        case EVX_PEEK_SOURCE: return convert_image(context.cache_bank.input_cache, &output_image);

        case EVX_PEEK_DESTINATION: 
        {
            uint32 dest_index = query_prediction_index_by_offset(frame, 1);
            return convert_image(context.cache_bank.prediction_cache[dest_index], &output_image);
        }

        case EVX_PEEK_BLOCK_TABLE:
        {
            for (uint32 j = 0; j < header.frame_height; j++)
            for (uint32 i = 0; i < header.frame_width; i++)
            {
                uint8 *out_data = output_image.query_data() + output_image.query_block_offset(i, j);
                uint32 block_index = (i / EVX_MACROBLOCK_SIZE) + (j / EVX_MACROBLOCK_SIZE) * context.width_in_blocks;
                evx_block_desc *entry = &context.block_table[block_index];
                
                // Pure red (255, 0, 0) is the most expensive type of block overall.
                // Pure blue (0, 0, 255) is the least expensive type of block overall. 
                // Pure white (255, 255, 255) is the least expensive intra block.
                // Pure black (0, 0, 0) is the least expensive inter block.

                out_data[2] = 255 * EVX_IS_COPY_BLOCK_TYPE(entry->block_type);
                out_data[1] = 255 * EVX_IS_MOTION_BLOCK_TYPE(entry->block_type);
                out_data[0] = 255 * EVX_IS_INTRA_BLOCK_TYPE(entry->block_type);
            }

        } break;

        case EVX_PEEK_QUANT_TABLE:
        {
            for (uint32 j = 0; j < header.frame_height; j++)
            for (uint32 i = 0; i < header.frame_width; i++)
            {
                uint8 *out_data = output_image.query_data() + output_image.query_block_offset(i, j);
                uint32 block_index = (i / EVX_MACROBLOCK_SIZE) + (j / EVX_MACROBLOCK_SIZE) * context.width_in_blocks;
                evx_block_desc *entry = &context.block_table[block_index];

                if (!EVX_IS_COPY_BLOCK_TYPE(entry->block_type))
                {
                    out_data[0] = 255 - 15 * entry->q_index;
                    out_data[1] = 255 - 15 * entry->q_index;
                    out_data[2] = 255 - 15 * entry->q_index;
                }
                else
                {
                    out_data[0] = 255;
                    out_data[1] = 0;
                    out_data[2] = 0;
                }
            }

        } break;

        case EVX_PEEK_BLOCK_VARIANCE:
        {
            for (uint32 j = 0; j < header.frame_height; j++)
            for (uint32 i = 0; i < header.frame_width; i++)
            {
                uint8 *out_data = output_image.query_data() + output_image.query_block_offset(i, j);
                uint32 block_index = (i / EVX_MACROBLOCK_SIZE) + (j / EVX_MACROBLOCK_SIZE) * context.width_in_blocks;
                evx_block_desc *entry = &context.block_table[block_index];

                if (!EVX_IS_COPY_BLOCK_TYPE(entry->block_type))
                {
                    int16 value = clip_range(entry->variance / 30, 0, 255);

                    out_data[0] = value;
                    out_data[1] = value;
                    out_data[2] = value;
                }
                else
                {
                    out_data[0] = 255;
                    out_data[1] = 0;
                    out_data[2] = 0;
                }
            }
        } break;

        case EVX_PEEK_SPMP_TABLE:
        {
            for (uint32 j = 0; j < header.frame_height; j++)
            for (uint32 i = 0; i < header.frame_width; i++)
            {
                uint8 *out_data = output_image.query_data() + output_image.query_block_offset(i, j);
                uint32 block_index = (i / EVX_MACROBLOCK_SIZE) + (j / EVX_MACROBLOCK_SIZE) * context.width_in_blocks;
                evx_block_desc *entry = &context.block_table[block_index];

                if (!entry->sp_pred)
                {
                    // No sub-pixel motion prediction.
                    out_data[0] = out_data[1] = out_data[2] = 0;
                }
                else
                {
                    // Blue = half pixel enabled
                    // Green = quarter pixel enabled

                    out_data[0] = 0;
                    out_data[1] = 255 * entry->sp_amount;
                    out_data[2] = 255 * !entry->sp_amount;
                }
            }

        } break;

        default: return EVX_ERROR_NOTIMPL;
    }

    return EVX_SUCCESS;
}

} // namespace evx