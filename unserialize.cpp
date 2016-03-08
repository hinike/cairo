
#include "base.h"
#include "scan.h"
#include "common.h"
#include "stream.h"
#include "macroblock.h"

namespace evx {

evx_status unserialize_block_8x8(bit_stream *input, int16 last_dc, bit_stream *feed_stream, entropy_coder *coder, int16 *cache, int16 *dest, uint32 dest_width)
{
    entropy_rle_stream_decode_8x8(input, coder, feed_stream, cache);

    cache[0] = cache[0] + last_dc;  // Reconstruct our dc using the delta value.

    for (uint32 j = 0; j < 8; j++)
    {
        aligned_byte_copy(cache + j * 8, sizeof(int16) * 8, dest + j * dest_width);
    }

    return EVX_SUCCESS; 
}

evx_status unserialize_block_16x16(bit_stream *input, int16 last_dc, bit_stream *feed_stream, entropy_coder *coder, int16 *cache, int16 *dest, uint32 dest_width)
{
    unserialize_block_8x8(input, last_dc, feed_stream, coder, cache, dest, dest_width);
    unserialize_block_8x8(input, dest[0], feed_stream, coder, cache, dest + 8, dest_width);
    unserialize_block_8x8(input, dest[0], feed_stream, coder, cache, dest + 8 * dest_width, dest_width);
    unserialize_block_8x8(input, dest[8 * dest_width], feed_stream, coder, cache, dest + 8 * dest_width + 8, dest_width);

    return EVX_SUCCESS;
}

evx_status unserialize_image_blocks_16x16(bit_stream *input, evx_block_desc *block_table, bit_stream *feed_stream, entropy_coder *coder, int16 *cache_data, image *dest_image)
{
    uint16 block_index = 0;
    uint32 width = dest_image->query_width();
    uint32 height = dest_image->query_height();

    feed_stream->empty();

    for (int32 j = 0; j < height; j += EVX_MACROBLOCK_SIZE)
    for (int32 i = 0; i < width; i += EVX_MACROBLOCK_SIZE)
    {
        evx_block_desc *block_desc = &block_table[block_index++];
        
        int16 last_dc = 0;
        int16 *last_block_data = NULL;
        int16 *block_data = reinterpret_cast<int16 *>(dest_image->query_data() + dest_image->query_block_offset(i, j));

        // Copy blocks contain no residuals.
        if (EVX_IS_COPY_BLOCK_TYPE(block_desc->block_type))
        {
            continue;
        }

        // Support delta dc coding
        if (i >= EVX_MACROBLOCK_SIZE)
        {
            last_block_data = reinterpret_cast<int16 *>(dest_image->query_data() + dest_image->query_block_offset(i - (EVX_MACROBLOCK_SIZE >> 1), j));
            last_dc = last_block_data[0];
        }
        else
        {
            if (j >= EVX_MACROBLOCK_SIZE)
            {
                // i is zero, so we sample from the block above.
                last_block_data = reinterpret_cast<int16 *>(dest_image->query_data() + dest_image->query_block_offset(i, j - (EVX_MACROBLOCK_SIZE >> 1)));
                last_dc = last_block_data[0];
            }
        }

        unserialize_block_16x16(input, last_dc, feed_stream, coder, cache_data, block_data, width);
    }

    return EVX_SUCCESS;
}

evx_status unserialize_image_blocks_8x8(bit_stream *input, evx_block_desc *block_table, bit_stream *feed_stream, entropy_coder *coder, int16 *cache_data, image *dest_image)
{
    uint16 block_index = 0;
    uint32 width = dest_image->query_width();
    uint32 height = dest_image->query_height();

    feed_stream->empty();
   
    for (int32 j = 0; j < height; j += (EVX_MACROBLOCK_SIZE >> 1))
    for (int32 i = 0; i < width; i += (EVX_MACROBLOCK_SIZE >> 1))
    {
        evx_block_desc *block_desc = &block_table[block_index++];

        int16 last_dc = 0;
        int16 *last_block_data = NULL;
        int16 *block_data = reinterpret_cast<int16 *>(dest_image->query_data() + dest_image->query_block_offset(i, j));

        // Copy blocks contain no residuals.
        if (EVX_IS_COPY_BLOCK_TYPE(block_desc->block_type))
        {
            continue;
        }

        // Support delta dc coding
        if (i >= (EVX_MACROBLOCK_SIZE >> 1))
        {
            last_block_data = reinterpret_cast<int16 *>(dest_image->query_data() + dest_image->query_block_offset(i - (EVX_MACROBLOCK_SIZE >> 1), j));
            last_dc = last_block_data[0];
        }
        else
        {
            if (j >= (EVX_MACROBLOCK_SIZE >> 1))
            {
                // i is zero, so we sample from the block above.
                last_block_data = reinterpret_cast<int16 *>(dest_image->query_data() + dest_image->query_block_offset(i, j - (EVX_MACROBLOCK_SIZE >> 1)));
                last_dc = last_block_data[0];
            }
        }

        unserialize_block_8x8(input, last_dc, feed_stream, coder, cache_data, block_data, width);
    }

    return EVX_SUCCESS;
}

evx_status unserialize_macroblocks(bit_stream *input, evx_context *context)
{
    image *y_image = context->cache_bank.input_cache.query_y_image();
    image *u_image = context->cache_bank.input_cache.query_u_image();
    image *v_image = context->cache_bank.input_cache.query_v_image();

    if (evx_failed(unserialize_image_blocks_16x16(input, context->block_table, &context->feed_stream,
                   &context->arith_coder, context->cache_bank.staging_block.data_y, y_image)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

#if EVX_ENABLE_CHROMA_SUPPORT

    if (evx_failed(unserialize_image_blocks_8x8(input, context->block_table, &context->feed_stream,
                   &context->arith_coder, context->cache_bank.staging_block.data_u, u_image)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (evx_failed(unserialize_image_blocks_8x8(input, context->block_table, &context->feed_stream,
                   &context->arith_coder, context->cache_bank.staging_block.data_v, v_image)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

#endif

    return EVX_SUCCESS;
}

evx_status unserialize_block_types(uint16 block_count, bit_stream *input, bit_stream *feed_stream, entropy_coder *coder, evx_block_desc *block_table)
{
    feed_stream->empty();

    for (uint32 i = 0; i < block_count; i++)
    {
        coder->decode(3, input, feed_stream, false);
        feed_stream->read_bits(&block_table[i].block_type, 3);
    }

    return EVX_SUCCESS;
}

evx_status unserialize_prediction_targets(uint16 block_count, bit_stream *input, bit_stream *feed_stream, entropy_coder *coder, evx_block_desc *block_table)
{
    feed_stream->empty();

    for (uint32 i = 0; i < block_count; i++)
    {
        if (EVX_IS_INTRA_BLOCK_TYPE(block_table[i].block_type))
        {
            continue;
        }

        uint8 bit_count = log2((uint8) EVX_REFERENCE_FRAME_COUNT);
        coder->decode(bit_count, input, feed_stream, false);
        feed_stream->read_bits(&block_table[i].prediction_target, bit_count);
    }

    return EVX_SUCCESS;
}

evx_status unserialize_motion_vectors(uint16 block_count, bit_stream *input, bit_stream *feed_stream, entropy_coder *coder, evx_block_desc *block_table)
{
    feed_stream->empty();
    int16 last_x = 0, last_y = 0;

    // Decode our x component differences.
    for (uint32 i = 0; i < block_count; i++)
    {
        if (!EVX_IS_MOTION_BLOCK_TYPE(block_table[i].block_type))
        {
            continue;
        }

        int16 current_x = 0;
        entropy_stream_decode_value(input, coder, feed_stream, &current_x);
        block_table[i].motion_x = last_x + current_x;
        last_x = block_table[i].motion_x;
    }

    // Decode our y component differences.
    for (uint32 i = 0; i < block_count; i++)
    {
        if (!EVX_IS_MOTION_BLOCK_TYPE(block_table[i].block_type))
        {
            continue;
        }

        int16 current_y = 0;
        entropy_stream_decode_value(input, coder, feed_stream, &current_y);
        block_table[i].motion_y = last_y + current_y;
        last_y = block_table[i].motion_y;
    }

    return EVX_SUCCESS;
}

evx_status unserialize_subpixel_motion_params(uint16 block_count, bit_stream *input, bit_stream *feed_stream, entropy_coder *coder, evx_block_desc *block_table)
{
    feed_stream->empty();

    // Subpixel prediction enabled bit
    for (uint32 i = 0; i < block_count; i++)
    {
        if (!EVX_IS_MOTION_BLOCK_TYPE(block_table[i].block_type))
        {
            continue;
        }

        coder->decode(1, input, feed_stream, false);
        feed_stream->read_bit(&block_table[i].sp_pred);
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

        coder->decode(1, input, feed_stream, false);
        feed_stream->read_bit(&block_table[i].sp_amount);
    }

    // Subpixel direction (degree) bits.
    for (uint32 i = 0; i < block_count; i++)
    {
        if (!EVX_IS_MOTION_BLOCK_TYPE(block_table[i].block_type) || 
            !block_table[i].sp_pred)
        {
            continue;
        }

        coder->decode(3, input, feed_stream, false);
        feed_stream->read_bits(&block_table[i].sp_index, 3);
    }

    return EVX_SUCCESS;
}

evx_status unserialize_block_quality(uint16 block_count, bit_stream *input, bit_stream *feed_stream, entropy_coder *coder, evx_block_desc *block_table)
{
    int16 last_q = 0; 
    feed_stream->empty();

    for (uint32 i = 0; i < block_count; i++)
    {
        if (EVX_IS_COPY_BLOCK_TYPE(block_table[i].block_type))
        {
            continue;
        }

        int16 current_q = 0;
        entropy_stream_decode_value(input, coder, feed_stream, &current_q);
        block_table[i].q_index = current_q + last_q;
        last_q = block_table[i].q_index;
    }

    return EVX_SUCCESS;
}

evx_status unserialize_block_table(uint16 block_count, bit_stream *input, bit_stream *feed_stream, entropy_coder *coder, evx_block_desc *block_table)
{
    if (evx_failed(unserialize_block_types(block_count, input, feed_stream, coder, block_table)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (evx_failed(unserialize_prediction_targets(block_count, input, feed_stream, coder, block_table)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (evx_failed(unserialize_motion_vectors(block_count, input, feed_stream, coder, block_table)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (evx_failed(unserialize_subpixel_motion_params(block_count, input, feed_stream, coder, block_table)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (evx_failed(unserialize_block_quality(block_count, input, feed_stream, coder, block_table)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    return EVX_SUCCESS;
}

evx_status unserialize_slice(bit_stream *input, evx_context *context)
{
    uint16 block_count = context->width_in_blocks * context->height_in_blocks;

    context->arith_coder.clear();
    context->arith_coder.start_decode(input);

    // Unserialize the encoded contents of our context, starting with the block table.
    if (evx_failed(unserialize_block_table(block_count, input, &context->feed_stream, &context->arith_coder, context->block_table)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    // Unserialize all transformed and quantized residuals.
    if (evx_failed(unserialize_macroblocks(input, context)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    return EVX_SUCCESS;
}

} // namespace evx