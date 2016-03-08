
#include "base.h"
#include "scan.h"
#include "common.h"
#include "stream.h"
#include "macroblock.h"

namespace evx {

evx_status serialize_block_8x8(int16 *source, uint32 source_width, int16 last_dc, int16 *cache, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    // Our entropy stream encode uses a zigzag pattern to efficiently encode our residuals. 
    // This requires a contiguous input buffer, so we copy from a non-contiguous source 
    // into a contiguous cache. 
    for (uint32 j = 0; j < 8; j++)
    {
        aligned_byte_copy(source + j * source_width, sizeof(int16) * 8, cache + j * 8);
    }

    cache[0] = cache[0] - last_dc;  // Compute and encode a delta value for the dc.

    return entropy_rle_stream_encode_8x8(cache, feed_stream, coder, output); 
}

evx_status serialize_block_16x16(int16 *source, uint32 source_width, int16 last_dc, int16 *cache, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    serialize_block_8x8(source, source_width, last_dc, cache, feed_stream, coder, output);
    serialize_block_8x8(source + 8, source_width, source[0], cache, feed_stream, coder, output);
    serialize_block_8x8(source + 8 * source_width, source_width, source[0], cache, feed_stream, coder, output);
    serialize_block_8x8(source + 8 * source_width + 8, source_width, source[8 * source_width], cache, feed_stream, coder, output);

    return EVX_SUCCESS;
}

evx_status serialize_image_blocks_16x16(image *source_image, int16 *cache_data, evx_block_desc *block_table, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    uint16 block_index = 0;
    uint32 width = source_image->query_width();
    uint32 height = source_image->query_height();

    feed_stream->empty();

    for (int32 j = 0; j < height; j += EVX_MACROBLOCK_SIZE)
    for (int32 i = 0; i < width; i += EVX_MACROBLOCK_SIZE)
    {
        evx_block_desc *block_desc = &block_table[block_index++];
        
        int16 last_dc = 0;
        int16 *last_block_data = NULL;
        int16 *block_data = reinterpret_cast<int16 *>(source_image->query_data() + source_image->query_block_offset(i, j));

        // Copy blocks contain no residuals.
        if (EVX_IS_COPY_BLOCK_TYPE(block_desc->block_type))
        {
            continue;
        }

        // Support delta dc coding
        if (i >= EVX_MACROBLOCK_SIZE)
        {
            last_block_data = reinterpret_cast<int16 *>(source_image->query_data() + source_image->query_block_offset(i - (EVX_MACROBLOCK_SIZE >> 1), j));
            last_dc = last_block_data[0];
        }
        else
        {
            if (j >= EVX_MACROBLOCK_SIZE)
            {
                // i is zero, so we sample from the block above.
                last_block_data = reinterpret_cast<int16 *>(source_image->query_data() + source_image->query_block_offset(i, j - (EVX_MACROBLOCK_SIZE >> 1)));
                last_dc = last_block_data[0];
            }
        }

        serialize_block_16x16(block_data, width, last_dc, cache_data, feed_stream, coder, output);
    }

    return EVX_SUCCESS;
}

evx_status serialize_image_blocks_8x8(image *source_image, int16 *cache_data, evx_block_desc *block_table, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    uint16 block_index = 0;
    uint32 width = source_image->query_width();
    uint32 height = source_image->query_height();

    feed_stream->empty();

    for (int32 j = 0; j < height; j += (EVX_MACROBLOCK_SIZE >> 1))
    for (int32 i = 0; i < width; i += (EVX_MACROBLOCK_SIZE >> 1))
    {
        evx_block_desc *block_desc = &block_table[block_index++];
        
        int16 last_dc = 0;
        int16 *last_block_data = NULL;
        int16 *block_data = reinterpret_cast<int16 *>(source_image->query_data() + source_image->query_block_offset(i, j));

        // Copy blocks contain no residuals.
        if (EVX_IS_COPY_BLOCK_TYPE(block_desc->block_type))
        {
            continue;
        }

        // Support delta dc coding
        if (i >= (EVX_MACROBLOCK_SIZE >> 1))
        {
            last_block_data = reinterpret_cast<int16 *>(source_image->query_data() + source_image->query_block_offset(i - (EVX_MACROBLOCK_SIZE >> 1), j));
            last_dc = last_block_data[0];
        }
        else
        {
            if (j >= (EVX_MACROBLOCK_SIZE >> 1))
            {
                // i is zero, so we sample from the block above.
                last_block_data = reinterpret_cast<int16 *>(source_image->query_data() + source_image->query_block_offset(i, j - (EVX_MACROBLOCK_SIZE >> 1)));
                last_dc = last_block_data[0];
            }
        }

        serialize_block_8x8(block_data, width, last_dc, cache_data, feed_stream, coder, output);
    }

    return EVX_SUCCESS;
}

evx_status serialize_macroblocks(evx_context *context, bit_stream *output)
{
    image *y_image = context->cache_bank.output_cache.query_y_image();
    image *u_image = context->cache_bank.output_cache.query_u_image();
    image *v_image = context->cache_bank.output_cache.query_v_image();

    if (evx_failed(serialize_image_blocks_16x16(y_image, context->cache_bank.staging_block.data_y, 
                   context->block_table, &context->feed_stream, &context->arith_coder, output)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

#if EVX_ENABLE_CHROMA_SUPPORT

    if (evx_failed(serialize_image_blocks_8x8(u_image, context->cache_bank.staging_block.data_u, 
                   context->block_table, &context->feed_stream, &context->arith_coder, output)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (evx_failed(serialize_image_blocks_8x8(v_image, context->cache_bank.staging_block.data_v, 
                   context->block_table, &context->feed_stream, &context->arith_coder, output))) 
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

#endif

    return EVX_SUCCESS;
}

evx_status serialize_block_types(uint16 block_count, evx_block_desc *block_table, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    feed_stream->empty();

    for (uint32 i = 0; i < block_count; i++)
    {
        feed_stream->write_bits(&block_table[i].block_type, 3);
    }

    return coder->encode(feed_stream, output, false);
}

evx_status serialize_prediction_targets(uint16 block_count, evx_block_desc *block_table, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    feed_stream->empty();

    for (uint32 i = 0; i < block_count; i++)
    {
        if (EVX_IS_INTRA_BLOCK_TYPE(block_table[i].block_type))
        {
            continue;
        }

        uint8 bit_count = log2((uint8) EVX_REFERENCE_FRAME_COUNT);
        feed_stream->write_bits(&block_table[i].prediction_target, bit_count);
    }

    return coder->encode(feed_stream, output, false);
}

evx_status serialize_motion_vectors(uint16 block_count, evx_block_desc *block_table, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    // We encode our motion vectors as motion vector differences, one component at a time.
    int16 last_x = 0, last_y = 0;
    feed_stream->empty();

    // Encode our x component differences.
    for (uint32 i = 0; i < block_count; i++)
    {
        if (!EVX_IS_MOTION_BLOCK_TYPE(block_table[i].block_type))
        {
            continue;
        }

        int16 current_x = block_table[i].motion_x - last_x;
        entropy_stream_encode_value(current_x, feed_stream, coder, output);
        last_x = block_table[i].motion_x;
    }

    // Encode our y component differences.
    for (uint32 i = 0; i < block_count; i++)
    {
        if (!EVX_IS_MOTION_BLOCK_TYPE(block_table[i].block_type))
        {
            continue;
        }

        int16 current_y = block_table[i].motion_y - last_y;
        entropy_stream_encode_value(current_y, feed_stream, coder, output);
        last_y = block_table[i].motion_y;
    }

    return EVX_SUCCESS;
}

evx_status serialize_subpixel_motion_params(uint16 block_count, evx_block_desc *block_table, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    feed_stream->empty();

    // Subpixel prediction enabled bit
    for (uint32 i = 0; i < block_count; i++)
    {
        if (!EVX_IS_MOTION_BLOCK_TYPE(block_table[i].block_type))
        {
            continue;
        }

        feed_stream->write_bit(block_table[i].sp_pred);
        coder->encode(feed_stream, output, false);
    }

    // Subpixel level bit.
    for (uint32 i = 0; i < block_count; i++)
    {
        // The subpixel direction and level are only emitted 
        // if subpixel prediction is enabled.
        if (!EVX_IS_MOTION_BLOCK_TYPE(block_table[i].block_type) || 
            !block_table[i].sp_pred)
        {
            continue;
        }

        feed_stream->write_bit(block_table[i].sp_amount);
        coder->encode(feed_stream, output, false);
    }

    // Subpixel direction (degree) bits.
    for (uint32 i = 0; i < block_count; i++)
    {
        if (!EVX_IS_MOTION_BLOCK_TYPE(block_table[i].block_type) || 
            !block_table[i].sp_pred)
        {
            continue;
        }
        
        feed_stream->write_bits(&block_table[i].sp_index, 3);
        coder->encode(feed_stream, output, false);
    }

    return EVX_SUCCESS;
}

evx_status serialize_block_quality(uint16 block_count, evx_block_desc *block_table, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    int16 last_q = 0; 
    feed_stream->empty();

    for (uint32 i = 0; i < block_count; i++)
    {
        if (EVX_IS_COPY_BLOCK_TYPE(block_table[i].block_type))
        {
            continue;
        }

        int16 current_q = block_table[i].q_index - last_q;
        stream_encode_value(current_q, feed_stream);
        last_q = block_table[i].q_index;
    }

    return coder->encode(feed_stream, output, false);
}

evx_status serialize_block_table(uint16 block_count, evx_block_desc *block_table, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    // Descriptors are serialized contiguously to improve efficiency.
    if (evx_failed(serialize_block_types(block_count, block_table, feed_stream, coder, output)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (evx_failed(serialize_prediction_targets(block_count, block_table, feed_stream, coder, output)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (evx_failed(serialize_motion_vectors(block_count, block_table, feed_stream, coder, output)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (evx_failed(serialize_subpixel_motion_params(block_count, block_table, feed_stream, coder, output)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (evx_failed(serialize_block_quality(block_count, block_table, feed_stream, coder, output)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    return EVX_SUCCESS;
}

evx_status serialize_slice(const evx_frame &frame, evx_context *context, bit_stream *output)
{
    uint16 block_count = context->width_in_blocks * context->height_in_blocks;

    context->arith_coder.clear();

    // Serialize the encoded contents of our context, starting with the block table.
    if (evx_failed(serialize_block_table(block_count, context->block_table, &context->feed_stream, &context->arith_coder, output)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }
    
    // Serialize all transformed and quantized residuals.
    if (evx_failed(serialize_macroblocks(context, output)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }
    
    context->arith_coder.finish_encode(output);

    return EVX_SUCCESS;
}

} // namespace evx