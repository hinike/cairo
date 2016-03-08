
#include "common.h"
#include "config.h"
#include "memory.h"
#include "version.h"

namespace evx {

evx_status initialize_header(uint32 width, uint32 height, evx_header *header)
{
    // Configure our header with default values.
    header->magic[0] = 'E';
    header->magic[1] = 'V';
    header->magic[2] = 'X';
    header->magic[3] = '1';
    header->ref_count = EVX_REFERENCE_FRAME_COUNT;
    header->version = EVX_VERSION_WORD(EVX_VERSION_MAJOR, EVX_VERSION_MINOR);
    header->frame_width = width;
    header->frame_height = height;
    header->size = sizeof(evx_header);

    return EVX_SUCCESS; 
}

evx_status verify_header(const evx_header &header)
{
    uint16 version = EVX_VERSION_WORD(EVX_VERSION_MAJOR, EVX_VERSION_MINOR);

    if (header.magic[0] != 'E' || header.magic[1] != 'V' ||
        header.magic[2] != 'X' || header.magic[3] != '1')
    {
        return EVX_ERROR_INVALID_RESOURCE;
    }

    if (header.version != version || 
        header.ref_count != EVX_REFERENCE_FRAME_COUNT || 
        header.size != sizeof(header))
    {
        return EVX_ERROR_INVALID_RESOURCE;
    }

    return EVX_SUCCESS;
}

evx_status clear_header(evx_header *header)
{
    return initialize_header(0, 0, header);
}

evx_status clear_frame(evx_frame *frame)
{
    if (EVX_PARAM_CHECK)
    {
        if (!frame)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    frame->type = EVX_FRAME_INTRA;
    frame->index = 0;
    frame->quality = clip_range(EVX_DEFAULT_QUALITY_LEVEL, 1, 100);

    return EVX_SUCCESS;
}

evx_status clear_block_desc(evx_block_desc *block_desc)
{
    aligned_zero_memory(block_desc, sizeof(block_desc));
    block_desc->block_type = EVX_BLOCK_INTRA_DEFAULT;

    return EVX_SUCCESS;
}

evx_context::evx_context() : block_table(NULL)
{
}

evx_status initialize_context(uint32 width, uint32 height, evx_context *context)
{
    if (EVX_PARAM_CHECK)
    {
        if (!context)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    context->width_in_blocks = (width >> EVX_MACROBLOCK_SHIFT);
    context->height_in_blocks = (height >> EVX_MACROBLOCK_SHIFT);
    uint32 block_count = (context->width_in_blocks) * (context->height_in_blocks);

    if (EVX_SUCCESS != context->cache_bank.input_cache.initialize(EVX_IMAGE_FORMAT_R16S, width, height))
    {
        clear_context(context);
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (EVX_SUCCESS != context->cache_bank.transform_cache.initialize(EVX_IMAGE_FORMAT_R16S, EVX_MACROBLOCK_SIZE, EVX_MACROBLOCK_SIZE))
    {
        clear_context(context);
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (EVX_SUCCESS != context->cache_bank.motion_cache.initialize(EVX_IMAGE_FORMAT_R16S, EVX_MACROBLOCK_SIZE, EVX_MACROBLOCK_SIZE))
    {
        clear_context(context);
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (EVX_SUCCESS != context->cache_bank.output_cache.initialize(EVX_IMAGE_FORMAT_R16S, width, height))
    {
        clear_context(context);
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (EVX_SUCCESS != context->cache_bank.staging_cache.initialize(EVX_IMAGE_FORMAT_R16S, EVX_MACROBLOCK_SIZE, EVX_MACROBLOCK_SIZE))
    {
        clear_context(context);
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    for (uint32 i = 0; i < EVX_REFERENCE_FRAME_COUNT; ++i)
    {
        if (EVX_SUCCESS != context->cache_bank.prediction_cache[i].initialize(EVX_IMAGE_FORMAT_R16S, width, height))
        {
            clear_context(context);
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }
    
    // Create block caches.
    create_macroblock(context->cache_bank.transform_cache, 0, 0, &context->cache_bank.transform_block);
    create_macroblock(context->cache_bank.motion_cache, 0, 0, &context->cache_bank.motion_block);
    create_macroblock(context->cache_bank.staging_cache, 0, 0, &context->cache_bank.staging_block);

    context->block_table = new evx_block_desc[block_count];

    if (!context->block_table)
    {
        clear_context(context);
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    aligned_zero_memory(context->block_table, sizeof(evx_block_desc) * block_count);

    context->feed_stream.resize_capacity(32 * EVX_MB);

    return EVX_SUCCESS;
}

evx_status clear_context(evx_context *context)
{
    if (EVX_PARAM_CHECK)
    {
        if (!context)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    context->cache_bank.input_cache.deinitialize();
    context->cache_bank.output_cache.deinitialize();
    context->cache_bank.transform_cache.deinitialize();
    context->cache_bank.motion_cache.deinitialize();
    context->cache_bank.staging_cache.deinitialize();

    for (uint32 i = 0; i < EVX_REFERENCE_FRAME_COUNT; ++i)
    {
        context->cache_bank.prediction_cache[i].deinitialize();
    }

    delete [] context->block_table;

    context->block_table = NULL;
    context->feed_stream.clear();
    context->arith_coder.clear();

    return EVX_SUCCESS;
}

uint32 query_context_width(const evx_context &context)
{
    return context.width_in_blocks << EVX_MACROBLOCK_SHIFT;
}

uint32 query_context_height(const evx_context &context)
{
    return context.height_in_blocks << EVX_MACROBLOCK_SHIFT;
}

uint32 query_prediction_index_by_offset(const evx_frame &frame, uint8 offset)
{
    return (frame.index + EVX_REFERENCE_FRAME_COUNT - offset) % EVX_REFERENCE_FRAME_COUNT;
}

} // namespace evx