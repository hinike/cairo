
#include "evx1.h"
#include "analysis.h"
#include "config.h"
#include "convert.h"
#include "evx1dec.h"
#include "motion.h"
#include "quantize.h"

namespace evx {

evx_status unserialize_slice(bit_stream *input, evx_context *context);
evx_status deblock_image_filter(evx_block_desc *block_table, image_set *target_image);

evx_status decode_block(const evx_frame &frame, const evx_block_desc &block_desc, const macroblock &source_block, 
                        evx_cache_bank *cache_bank, int32 i, int32 j, macroblock *dest_block)
{
    switch (block_desc.block_type)
    {
        case EVX_BLOCK_INTRA_DEFAULT:
        {
            inverse_quantize_macroblock(block_desc.q_index, block_desc.block_type, source_block, &cache_bank->transform_block);
            inverse_transform_macroblock(cache_bank->transform_block, dest_block);

        } break;

        case EVX_BLOCK_INTRA_MOTION_COPY:
        {
            macroblock beta_block;
            uint32 intra_pred_index = query_prediction_index_by_offset(frame, 0);
            create_macroblock(cache_bank->prediction_cache[intra_pred_index], i + block_desc.motion_x, j + block_desc.motion_y, &beta_block);

            if (block_desc.sp_pred)
            {
                int16 sp_i, sp_j;
                compute_motion_direction_from_frac_index(block_desc.sp_index, &sp_i, &sp_j);
                create_subpixel_macroblock(&cache_bank->prediction_cache[intra_pred_index], block_desc.sp_amount, beta_block, 
                                           i + block_desc.motion_x + sp_i, j + block_desc.motion_y + sp_j, &cache_bank->motion_block);

                copy_macroblock(cache_bank->motion_block, dest_block);
            }
            else 
            {
                // no sub-pixel motion estimation
                copy_macroblock(beta_block, dest_block);
            }

        } break;

        case EVX_BLOCK_INTRA_MOTION_DELTA:
        {
            macroblock beta_block;
            uint32 intra_pred_index = query_prediction_index_by_offset(frame, 0); 
            create_macroblock(cache_bank->prediction_cache[intra_pred_index], i + block_desc.motion_x, j + block_desc.motion_y, &beta_block);
            inverse_quantize_macroblock(block_desc.q_index, block_desc.block_type, source_block, &cache_bank->transform_block);

            if (block_desc.sp_pred)
            {
                int16 sp_i, sp_j;
                compute_motion_direction_from_frac_index(block_desc.sp_index, &sp_i, &sp_j);
                create_subpixel_macroblock(&cache_bank->prediction_cache[intra_pred_index], block_desc.sp_amount, beta_block, 
                                           i + block_desc.motion_x + sp_i, j + block_desc.motion_y + sp_j, &cache_bank->motion_block);

                inverse_transform_add_macroblock(cache_bank->transform_block, cache_bank->motion_block, dest_block);
            }
            else 
            {
                // no sub-pixel motion estimation
                inverse_transform_add_macroblock(cache_bank->transform_block, beta_block, dest_block);
            }

        } break;

        case EVX_BLOCK_INTER_MOTION_COPY:
        {
            macroblock beta_block;
            uint32 inter_pred_index = query_prediction_index_by_offset(frame, block_desc.prediction_target);
            create_macroblock(cache_bank->prediction_cache[inter_pred_index], i + block_desc.motion_x, j + block_desc.motion_y, &beta_block);

            if (block_desc.sp_pred)
            {
                int16 sp_i, sp_j;
                compute_motion_direction_from_frac_index(block_desc.sp_index, &sp_i, &sp_j);
                create_subpixel_macroblock(&cache_bank->prediction_cache[inter_pred_index], block_desc.sp_amount, beta_block, 
                                           i + block_desc.motion_x + sp_i, j + block_desc.motion_y + sp_j, &cache_bank->motion_block);

                copy_macroblock(cache_bank->motion_block, dest_block);
            }
            else 
            {
                // no sub-pixel motion estimation
                copy_macroblock(beta_block, dest_block);
            }

        } break;

        case EVX_BLOCK_INTER_COPY:
        {
            macroblock beta_block;
            uint32 inter_pred_index = query_prediction_index_by_offset(frame, block_desc.prediction_target);
            create_macroblock(cache_bank->prediction_cache[inter_pred_index], i, j, &beta_block);
            copy_macroblock(beta_block, dest_block);

        } break;

        case EVX_BLOCK_INTER_MOTION_DELTA:
        {
            macroblock beta_block;
            uint32 inter_pred_index = query_prediction_index_by_offset(frame, block_desc.prediction_target); 
            create_macroblock(cache_bank->prediction_cache[inter_pred_index], i + block_desc.motion_x, j + block_desc.motion_y, &beta_block);
            inverse_quantize_macroblock(block_desc.q_index, block_desc.block_type, source_block, &cache_bank->transform_block);

            if (block_desc.sp_pred)
            {
                int16 sp_i, sp_j;
                compute_motion_direction_from_frac_index(block_desc.sp_index, &sp_i, &sp_j);
                create_subpixel_macroblock(&cache_bank->prediction_cache[inter_pred_index], block_desc.sp_amount, beta_block, 
                                           i + block_desc.motion_x + sp_i, j + block_desc.motion_y + sp_j, &cache_bank->motion_block);

                inverse_transform_add_macroblock(cache_bank->transform_block, cache_bank->motion_block, dest_block);
            }
            else 
            {
                // no sub-pixel motion estimation
                inverse_transform_add_macroblock(cache_bank->transform_block, beta_block, dest_block);
            }

        } break;

        case EVX_BLOCK_INTER_DELTA:
        {
            macroblock beta_block;
            uint32 inter_pred_index = query_prediction_index_by_offset(frame, block_desc.prediction_target); 
            create_macroblock(cache_bank->prediction_cache[inter_pred_index], i, j, &beta_block);
            inverse_quantize_macroblock(block_desc.q_index, block_desc.block_type, source_block, &cache_bank->transform_block);
            inverse_transform_add_macroblock(cache_bank->transform_block, beta_block, dest_block);

        } break;

        default: return evx_post_error(EVX_ERROR_INVALID_RESOURCE);;
    };

    return EVX_SUCCESS;
}

evx_status decode_slice(const evx_frame &frame, evx_context *context)
{
    uint32 block_index = 0;
    uint32 width = query_context_width(*context);
    uint32 height = query_context_height(*context);
    uint32 dest_index = query_prediction_index_by_offset(frame, 0);

    macroblock source_block, dest_block;

    for (int32 j = 0; j < height; j += EVX_MACROBLOCK_SIZE)
    for (int32 i = 0; i < width;  i += EVX_MACROBLOCK_SIZE)
    {
        evx_block_desc *block_desc = &context->block_table[block_index++];
        
        create_macroblock(context->cache_bank.input_cache, i, j, &source_block);
        create_macroblock(context->cache_bank.prediction_cache[dest_index], i, j, &dest_block);

        if (evx_failed(decode_block(frame, *block_desc, source_block, &context->cache_bank, i, j, &dest_block)))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    return EVX_SUCCESS;
}

evx_status engine_decode_frame(bit_stream *input, const evx_frame &frame, evx_context *context, image *output)
{
    uint32 dest_index = query_prediction_index_by_offset(frame, 0);

    if (evx_failed(unserialize_slice(input, context)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (evx_failed(decode_slice(frame, context)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    // Run our in-loop deblocking filter on the final post prediction image.
    if (evx_failed(deblock_image_filter(context->block_table, &context->cache_bank.prediction_cache[dest_index])))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (evx_failed(convert_image(context->cache_bank.prediction_cache[dest_index], output)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    return EVX_SUCCESS;
}

} // namespace evx