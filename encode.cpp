
#include "evx1.h"
#include "analysis.h"
#include "config.h"
#include "convert.h"
#include "evx1enc.h"
#include "motion.h"
#include "quantize.h"

namespace evx {

evx_status deblock_image_filter(evx_block_desc *block_table, image_set *target_image);
evx_status serialize_slice(const evx_frame &frame, evx_context *context, bit_stream *output);
evx_status decode_block(const evx_frame &frame, const evx_block_desc &block_desc, const macroblock &source_block, 
                        evx_cache_bank *cache_bank, int32 i, int32 j, macroblock *dest_block);

evx_status classify_block(const evx_frame &frame, const macroblock &source_block, evx_cache_bank *cache_bank, int32 i, int32 j, evx_block_desc *output)
{
    evx_block_desc best_desc;

    // calculate_intra_prediction will return the source block desc if there is no better intra alternative.
    int32 best_sad = calculate_intra_prediction(frame, source_block, i, j, cache_bank, &best_desc);

    if (EVX_FRAME_INTER == frame.type)
    {
        // This loop will prioritize closer predictions over far. The further the prediction index
        // the more costly it becomes to encode it, so we should require increasingly higher thresholds
        // for further predictions.
        for (uint8 offset = 1; offset < EVX_REFERENCE_FRAME_COUNT; ++offset)
        {
            evx_block_desc inter_desc;

            // calculate_inter_prediction will always return a best candidate.
            int32 inter_sad = calculate_inter_prediction(frame, source_block, i, j, cache_bank, offset, &inter_desc);
            
            if (EVX_IS_COPY_BLOCK_TYPE(inter_desc.block_type) ^ EVX_IS_COPY_BLOCK_TYPE(best_desc.block_type))
            {
                // Always keep the copy block.
                if (EVX_IS_COPY_BLOCK_TYPE(inter_desc.block_type))
                {
                    best_desc = inter_desc;  
                    best_sad = inter_sad;
                }
            }
            else
            {
                // If both blocks have the same copy status, then always take the
                // one with the lowest sad.
                if (inter_sad < best_sad)
                {
                    best_desc = inter_desc;  
                    best_sad = inter_sad;
                }
            }
        }
    }

    if (best_sad == EVX_MAX_INT32)
    {
        // Critial error during motion estimation.
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    *output = best_desc;

    return EVX_SUCCESS;
}

evx_status encode_block(const evx_frame &frame, const macroblock &source_block, evx_cache_bank *cache_bank, 
                        int32 i, int32 j, evx_block_desc *block_desc, macroblock *dest_block)
{
    // Classification only performs a fast block comparison and interpolation, so we recalculate
    // the full block values when necessary.

    switch (block_desc->block_type)
    {
        case EVX_BLOCK_INTRA_DEFAULT:
        {
            transform_macroblock(source_block, &cache_bank->transform_block);
            block_desc->q_index = query_block_quantization_parameter(frame.quality, cache_bank->transform_block, block_desc->block_type);
            block_desc->variance = compute_block_variance2(cache_bank->transform_block);
            quantize_macroblock(block_desc->q_index, block_desc->block_type, cache_bank->transform_block, dest_block);

        } break;

        case EVX_BLOCK_INTRA_MOTION_DELTA:
        {
            macroblock beta_block;
            uint32 intra_pred_index = query_prediction_index_by_offset(frame, 0);
            create_macroblock(cache_bank->prediction_cache[intra_pred_index], i + block_desc->motion_x, j + block_desc->motion_y, &beta_block);

            if (block_desc->sp_pred)
            {
                int16 sp_i, sp_j;
                compute_motion_direction_from_frac_index(block_desc->sp_index, &sp_i, &sp_j);
                create_subpixel_macroblock(&cache_bank->prediction_cache[intra_pred_index], block_desc->sp_amount, beta_block, 
                                           i + block_desc->motion_x + sp_i, j + block_desc->motion_y + sp_j, &cache_bank->motion_block);

                sub_transform_macroblock(source_block, cache_bank->motion_block, &cache_bank->transform_block);
            }
            else 
            {
                // no sub-pixel motion estimation
                sub_transform_macroblock(source_block, beta_block, &cache_bank->transform_block);
            }

            block_desc->q_index = query_block_quantization_parameter(frame.quality, cache_bank->transform_block, block_desc->block_type);
            block_desc->variance = compute_block_variance2(cache_bank->transform_block);
            quantize_macroblock(block_desc->q_index, block_desc->block_type, cache_bank->transform_block, dest_block);

        } break;

        case EVX_BLOCK_INTER_DELTA:
        {
            macroblock beta_block;
            uint32 inter_pred_index = query_prediction_index_by_offset(frame, block_desc->prediction_target);
            create_macroblock(cache_bank->prediction_cache[inter_pred_index], i, j, &beta_block);

            sub_transform_macroblock(source_block, beta_block, &cache_bank->transform_block);

            block_desc->q_index = query_block_quantization_parameter(frame.quality, cache_bank->transform_block, block_desc->block_type);
            block_desc->variance = compute_block_variance2(cache_bank->transform_block);
            quantize_macroblock(block_desc->q_index, block_desc->block_type, cache_bank->transform_block, dest_block);

        } break;

        case EVX_BLOCK_INTER_MOTION_DELTA:
        {
            macroblock beta_block;
            uint32 inter_pred_index = query_prediction_index_by_offset(frame, block_desc->prediction_target);
            create_macroblock(cache_bank->prediction_cache[inter_pred_index], i + block_desc->motion_x, j + block_desc->motion_y, &beta_block);

            if (block_desc->sp_pred)
            {
                int16 sp_i, sp_j;
                compute_motion_direction_from_frac_index(block_desc->sp_index, &sp_i, &sp_j);
                create_subpixel_macroblock(&cache_bank->prediction_cache[inter_pred_index], block_desc->sp_amount, beta_block, 
                                           i + block_desc->motion_x + sp_i, j + block_desc->motion_y + sp_j, &cache_bank->motion_block);

                sub_transform_macroblock(source_block, cache_bank->motion_block, &cache_bank->transform_block);
            }
            else 
            {
                // no sub-pixel motion estimation
                sub_transform_macroblock(source_block, beta_block, &cache_bank->transform_block);
            }

            block_desc->q_index = query_block_quantization_parameter(frame.quality, cache_bank->transform_block, block_desc->block_type);
            block_desc->variance = compute_block_variance2(cache_bank->transform_block);
            quantize_macroblock(block_desc->q_index, block_desc->block_type, cache_bank->transform_block, dest_block);

        } break;

        // The following are primarily handled by the reverse (decode) pipeline.
        case EVX_BLOCK_INTRA_MOTION_COPY:    
        case EVX_BLOCK_INTER_MOTION_COPY:
        case EVX_BLOCK_INTER_COPY: break;
        
        default: return evx_post_error(EVX_ERROR_INVALID_RESOURCE);;
    }; 

    return EVX_SUCCESS;
}

evx_status encode_slice(const evx_frame &frame, evx_context *context)
{
    uint32 block_index = 0;
    uint32 width = query_context_width(*context);
    uint32 height = query_context_height(*context);
    uint32 dest_index = query_prediction_index_by_offset(frame, 0);

    macroblock source_block, dest_block, dest_prediction_block;

    for (int32 j = 0; j < height; j += EVX_MACROBLOCK_SIZE)
    for (int32 i = 0; i < width;  i += EVX_MACROBLOCK_SIZE)
    {
        evx_block_desc *block_desc = &context->block_table[block_index++];

        create_macroblock(context->cache_bank.input_cache, i, j, &source_block);
        create_macroblock(context->cache_bank.output_cache, i, j, &dest_block);
        create_macroblock(context->cache_bank.prediction_cache[dest_index], i, j, &dest_prediction_block);
         
        // Classify the block and pass it to the encoding pipeline.
        if (evx_failed(classify_block(frame, source_block, &context->cache_bank, i, j, block_desc)))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }

        if (evx_failed(encode_block(frame, source_block, &context->cache_bank, i, j, block_desc, &dest_block)))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }

        // The decoder frontend is used as our reverse pipeline. it would be more efficient to 
        // update our prediction within encode_block, but we sacrifice for clarity.
        if (evx_failed(decode_block(frame, *block_desc, dest_block, &context->cache_bank, i, j, &dest_prediction_block)))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    return EVX_SUCCESS;
}

evx_status engine_encode_frame(const image &input, const evx_frame &frame_desc, evx_context *context, bit_stream *output)
{
    uint32 dest_index = query_prediction_index_by_offset(frame_desc, 0);

    if (evx_failed(convert_image(input, &context->cache_bank.input_cache)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }
    
    if (evx_failed(encode_slice(frame_desc, context)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    // Serialize our context to the output bitstream.
    if (evx_failed(serialize_slice(frame_desc, context, output)))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    // Run our in-loop deblocking filter on the final post prediction image.
    if (evx_failed(deblock_image_filter(context->block_table, &context->cache_bank.prediction_cache[dest_index])))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    return EVX_SUCCESS;
}

} // namespace evx