
#include "analysis.h"
#include "common.h"
#include "macroblock.h"

// Increasing these above zero will allow the encoder to skip blocks that 
// exhibit some degree of variation. Note, this constant has been replaced by
// a quality dependent threshold (see mad_skip_threshold).

// #define EVX_MOTION_MAD_THRESHOLD                 ((EVX_DEFAULT_QUALITY_LEVEL >> 2) + 1)

// Intra prediction searches within the current frame for the closest possible match.
// A motion predicted block carries the overhead of motion vectors, so we only want 
// to use them when they are worth it (EVX_MOTION_MAD_THRESHOLD).
//
// Also, if a motion predicted block is visually identical to our current block, then
// we flag it as a skip block with motion prediction.

#define EVX_MOTION_SAD_THRESHOLD                 (8*EVX_KB)

// Defines our maximum search area around each block. Larger values will require
// significantly more processing but may detect larger movement more effectively.

#define EVX_MOTION_SEARCH_RADIUS                 (16)

namespace evx {

// Sum of Absolute Differences vs Maximum Absolute Difference
//
// We rely upon a SAD operation to determine the closest overall block to our input. 
// This identifies for us the block that will cost the least (overall) to use as our
// prediction. Once we've chosen this block we analyze it using a MAD to determine 
// if it contains any significant enough discrepancies to warrant full use as a non-skip
// block.

#define EVX_PIXEL_DISTANCE_SQ(sx, sy, dx, dy) ((sx-dx)*(sx-dx) + (sy-dy)*(sy-dy))  

typedef struct evx_prediction_params
{
    image_set *prediction;
    int16 mad_skip_threshold;
    int16 pixel_x;
    int16 pixel_y;

} evx_prediction_params;

typedef struct evx_motion_selection
{
    int16 best_x;
    int16 best_y;
    int32 best_sad;
    int32 best_mad;
    int32 best_ssd;
    int16 sp_index;

    bool sp_amount;
    bool sp_enabled;

} evx_motion_selection;

static inline int16 compute_motion_frac_index_from_direction(int16 i, int16 j)
{
    i++, j++;
    
    switch (j)
    {
        case 0: return i;
        case 1:
        {
            switch (i)
            {
                case 0: return 3;
                case 2: return 4;
            };

        } break;

        case 2: return i+5;
    };

    evx_post_error(EVX_ERROR_INVALIDARG);

    return 0;
}

void compute_motion_direction_from_frac_index(int16 frac_index, int16 *dir_x, int16 *dir_y)
{
    switch (frac_index)
    {
        case 0:
        case 1:
        case 2:
            *dir_y = -1;
            *dir_x = frac_index - 1;
            return;

        case 3: *dir_x = -1; *dir_y = 0; return;
        case 4: *dir_x = 1; *dir_y = 0; return;

        case 5:
        case 6:
        case 7:
            *dir_y = 1;
            *dir_x = frac_index - 6;
            return;
    };

    evx_post_error(EVX_ERROR_INVALIDARG);
}

static inline void evaluate_motion_candidate(int32 current_x, int32 current_y, const evx_prediction_params &params, 
                                             const macroblock &src_block, evx_motion_selection *selection)
{
    macroblock test_block;

    create_macroblock(*params.prediction, current_x, current_y, &test_block);  
    int32 current_sad = compute_block_sad(src_block, test_block); 
    int32 current_ssd = EVX_PIXEL_DISTANCE_SQ(current_x, current_y, params.pixel_x, params.pixel_y);
    int32 current_mad = compute_block_mad(src_block, test_block);

    // If we've already found a suitable copy block, then we only accept new 
    // candidates that are a closer matched copy block.
    if (selection->best_mad < params.mad_skip_threshold)
    {
        if (current_mad < selection->best_mad || 
            (current_mad == selection->best_mad && 
             current_ssd < selection->best_ssd))
        {
            selection->best_x = current_x;                                                                         
            selection->best_y = current_y;                                                                         
            selection->best_sad = current_sad;                         
            selection->best_ssd = current_ssd;
            selection->best_mad = current_mad; 
        }
    }
    else
    {
        if ((current_sad < selection->best_sad ||
            (current_sad == selection->best_sad && current_ssd < selection->best_ssd) && 
            current_sad < EVX_MOTION_SAD_THRESHOLD) || current_mad < params.mad_skip_threshold)                                                             
        {                                                                                               
            selection->best_x = current_x;                                                                         
            selection->best_y = current_y;                                                                         
            selection->best_sad = current_sad;                         
            selection->best_ssd = current_ssd;
            selection->best_mad = compute_block_mad(src_block, test_block);                                                                     
        }  
    } 
}

static inline void evaluate_subpel_motion_candidate(int32 target_x, int32 target_y, int16 i, int16 j, const evx_prediction_params &params, 
                                                    const macroblock &src_block, macroblock *cache_block, const macroblock &best_block,
                                                    evx_motion_selection *selection)
{
    macroblock test_block;

    // create an interpolated block between best_block and test_block using a half-pel lerp.
    create_macroblock(*params.prediction, target_x, target_y, &test_block);   
    lerp_macroblock_half(best_block, test_block, cache_block);                                                                       
    int32 current_sad = compute_block_sad(src_block, *cache_block); 
    int32 current_mad = compute_block_mad(src_block, *cache_block);
     
    // If we've already found a suitable copy block, then we only accept new 
    // candidates that are a closer matched copy block.
    if (selection->best_mad < params.mad_skip_threshold)
    {
        if (current_mad < selection->best_mad)
        {
            selection->sp_enabled = true;
            selection->sp_amount = false;   // identifies a half pixel interpolation.
            selection->sp_index = compute_motion_frac_index_from_direction(i, j);
 
            selection->best_sad = current_sad;                                                                     
            selection->best_mad = current_mad; 
        }
    }
    else
    {
        if ((current_sad < selection->best_sad && current_sad < EVX_MOTION_SAD_THRESHOLD) || 
            current_mad < params.mad_skip_threshold)                                                          
        {                                                                                               
            selection->sp_enabled = true;
            selection->sp_amount = false;   // identifies a half pixel interpolation.
            selection->sp_index = compute_motion_frac_index_from_direction(i, j);
 
            selection->best_sad = current_sad;                                                                     
            selection->best_mad = current_mad;                                                                    
        }  
    }                                                                                  
                                                                                                        
    // perform a quarter-pel lerp and compare the results.                                                                                     
    lerp_macroblock_quarter(best_block, test_block, cache_block);                                                                   
    current_sad = compute_block_sad(src_block, *cache_block);     
    current_mad = compute_block_mad(src_block, *cache_block);
                             
    // If we've already found a suitable copy block, then we only accept new 
    // candidates that are a closer matched copy block.
    if (selection->best_mad < params.mad_skip_threshold)
    {
        if (current_mad < selection->best_mad)
        {
            selection->sp_enabled = true;
            selection->sp_amount = true;   // identifies a quarter pixel interpolation.
            selection->sp_index = compute_motion_frac_index_from_direction(i, j);
 
            selection->best_sad = current_sad;                                                                     
            selection->best_mad = current_mad;    
        }
    }
    else
    {
        if ((current_sad < selection->best_sad && current_sad < EVX_MOTION_SAD_THRESHOLD) || 
            current_mad < params.mad_skip_threshold)                                                           
        {                                                                                               
            selection->sp_enabled = true;
            selection->sp_amount = true;   // identifies a quarter pixel interpolation.
            selection->sp_index = compute_motion_frac_index_from_direction(i, j);
 
            selection->best_sad = current_sad;                                                                     
            selection->best_mad = current_mad;                                                                      
        }  
    }
}

void perform_intra_motion_search(int16 left, int16 top, int16 right, int16 bottom, int16 step, 
                                 const evx_prediction_params &params, const macroblock &src_block, 
                                 evx_motion_selection *selection)
{
    int32 base_x = selection->best_x;                                                                              
    int32 base_y = selection->best_y;                                                                              
                                                                                                        
    for (int16 j = top; j <= bottom; j += step)                                                         
    for (int16 i = left; i <= right; i += step)                                                         
    {                                                                                                   
        int32 current_x = base_x + i;                                                                   
        int32 current_y = base_y + j;                                                                   
                                                                                                        
        if (current_y > (params.pixel_y - EVX_MACROBLOCK_SIZE) && 
            current_x > (params.pixel_x - EVX_MACROBLOCK_SIZE)) 
        {                                                                                               
             continue;                                                                                  
        }                                                                                               
                                                                                                        
        if (current_x < 0 || current_x > params.prediction->query_width() - EVX_MACROBLOCK_SIZE ||              
            current_y < 0 || current_y > params.prediction->query_height() - EVX_MACROBLOCK_SIZE)               
        {                                                                                               
             continue;                                                                                  
        }                                                                                               
                                                                                                        
        evaluate_motion_candidate(current_x, current_y, params, src_block, selection);
    } 
}

void perform_inter_motion_search(int16 left, int16 top, int16 right, int16 bottom, int16 step, 
                                 const evx_prediction_params &params, const macroblock &src_block, 
                                 evx_motion_selection *selection)
{
    int32 base_x = selection->best_x;                                                                              
    int32 base_y = selection->best_y;                                                                              
                                                                                                        
    for (int16 j = top; j <= bottom; j += step)                                                         
    for (int16 i = left; i <= right; i += step)                                                         
    {                                                                                                   
        int32 current_x = base_x + i;                                                                   
        int32 current_y = base_y + j;                                                                                                                                                              
                                                                                                        
        if (current_x < 0 || current_x > params.prediction->query_width() - EVX_MACROBLOCK_SIZE ||              
            current_y < 0 || current_y > params.prediction->query_height() - EVX_MACROBLOCK_SIZE)               
        {                                                                                               
             continue;                                                                                  
        }                                                                                               
                                                                                                        
        evaluate_motion_candidate(current_x, current_y, params, src_block, selection);                                                                                             
    } 
}

void perform_intra_subpixel_motion_search(const evx_prediction_params &params, 
                                          const macroblock &src_block, 
                                          macroblock *cache_block,
                                          evx_motion_selection *selection)
{
    macroblock best_block;  // determined by pixel level motion analysis.
    create_macroblock(*params.prediction, selection->best_x, selection->best_y, &best_block);

    // ensure that the selection will contain no subpixel prediction if a better match is not found.
    selection->sp_index = 0;
    selection->sp_amount = false;
    selection->sp_enabled = false;

    // perform a search of neighboring subpixel blocks using biliear interpolation.
    for (int16 j = -1; j <= 1; ++j)                                                                     
    for (int16 i = -1; i <= 1; ++i)                                                                     
    {                                                                                                   
        int32 target_x = selection->best_x + i;                                                                    
        int32 target_y = selection->best_y + j;                                                                    
                                                                                                        
        if (0 == i && 0 == j)                                                                           
        {                                                                                               
            continue;                                                                                   
        }                                                                                               
        
        // if we've stepped beyond the stable portion of our intra frame, continue.
        if (target_y > (params.pixel_y - EVX_MACROBLOCK_SIZE) && 
            target_x > (params.pixel_x - EVX_MACROBLOCK_SIZE))   
        {                                                                                               
             continue;                                                                                  
        }                                                                                               
                                                                                                        
        if (target_x < 0 || target_x > params.prediction->query_width() - EVX_MACROBLOCK_SIZE ||                
            target_y < 0 || target_y > params.prediction->query_height() - EVX_MACROBLOCK_SIZE)                 
        {                                                                                               
            continue;                                                                                   
        }                                                                                               

        evaluate_subpel_motion_candidate(target_x, target_y, i, j, params, src_block, cache_block, best_block, selection);
    } 
}

void perform_inter_subpixel_motion_search(const evx_prediction_params &params, 
                                          const macroblock &src_block, 
                                          macroblock *cache_block,
                                          evx_motion_selection *selection)
{
    macroblock best_block;  // determined by pixel level motion analysis.
    create_macroblock(*params.prediction, selection->best_x, selection->best_y, &best_block);

    // ensure that the selection will contain no subpixel prediction if a better match is not found.
    selection->sp_index = 0;
    selection->sp_amount = false;
    selection->sp_enabled = false;

    // perform a search of neighboring subpixel blocks using biliear interpolation.
    for (int16 j = -1; j <= 1; ++j)                                                                     
    for (int16 i = -1; i <= 1; ++i)                                                                     
    {                                                                                                   
        int32 target_x = selection->best_x + i;                                                                    
        int32 target_y = selection->best_y + j;                                                                    
                                                                                                        
        if (0 == i && 0 == j)                                                                           
        {                                                                                               
            continue;                                                                                   
        }                                                                                                                                                                                             
                                                                                                        
        if (target_x < 0 || target_x > params.prediction->query_width() - EVX_MACROBLOCK_SIZE ||                
            target_y < 0 || target_y > params.prediction->query_height() - EVX_MACROBLOCK_SIZE)                 
        {                                                                                               
            continue;                                                                                   
        }                                                                                               
                                                                                
        evaluate_subpel_motion_candidate(target_x, target_y, i, j, params, src_block, cache_block, best_block, selection);
    } 
}

int32 calculate_intra_prediction(const evx_frame &frame, const macroblock &src_block, int32 pixel_x, int32 pixel_y, evx_cache_bank *cache_bank, evx_block_desc *output_desc)
{
    evx_motion_selection selection;
    selection.best_x = pixel_x;
    selection.best_y = pixel_y;
    selection.best_sad = compute_block_sad(src_block);
    selection.best_mad = EVX_MAX_INT32;
    selection.best_ssd = EVX_MAX_INT32;
    selection.sp_amount = 0;
    selection.sp_index = 0;
    selection.sp_enabled = false;

    evx_prediction_params params;
    params.pixel_x = pixel_x;
    params.pixel_y = pixel_y;
    params.mad_skip_threshold = ((frame.quality >> 2) + 1);

    // Search for the closest match to our current source block within the prediction image.
    uint32 intra_pred_index = query_prediction_index_by_offset(frame, 0);
    params.prediction = const_cast<image_set *>(&cache_bank->prediction_cache[intra_pred_index]);

    // Scan the following values in a triangle around our pixel:
    //                                                           
    //      X          X          X                                                  
    //                                                        
    //      X          X          X                                                   
    //                                                        
    //      X        Pixel  

    // initial scan around our current pixel
    perform_intra_motion_search(-EVX_MOTION_SEARCH_RADIUS, -(EVX_MOTION_SEARCH_RADIUS << 1), 
                                EVX_MOTION_SEARCH_RADIUS, 0, EVX_MOTION_SEARCH_RADIUS, 
                                params, src_block, &selection);  

    for (int16 i = (EVX_MOTION_SEARCH_RADIUS >> 1); i > 0; i >>= 1)
    {
        perform_intra_motion_search(-i, -i, i, i, i, params, src_block, &selection);
    }

    // perform sub-pixel motion estimation
    perform_intra_subpixel_motion_search(params, src_block, &cache_bank->motion_block, &selection);    

    // Fill out our block descriptor using our closest match.
    clear_block_desc(output_desc);

    EVX_SET_INTRA_BLOCK_TYPE_BIT(output_desc->block_type, true);

    if (selection.best_x != pixel_x || selection.best_y != pixel_y || selection.sp_enabled)
    {
        EVX_SET_MOTION_BLOCK_TYPE_BIT(output_desc->block_type, true);
    }

    if (selection.best_mad < params.mad_skip_threshold)
    {
        EVX_SET_COPY_BLOCK_TYPE_BIT(output_desc->block_type, true);
    }

    output_desc->prediction_target = 0;
    output_desc->motion_x = selection.best_x - pixel_x;
    output_desc->motion_y = selection.best_y - pixel_y;
    output_desc->sp_pred = selection.sp_enabled;
    output_desc->sp_amount = selection.sp_amount;
    output_desc->sp_index = selection.sp_index;

    return selection.best_sad;
}

int32 calculate_inter_prediction(const evx_frame &frame, const macroblock &src_block, int32 pixel_x, int32 pixel_y, evx_cache_bank *cache_bank, uint16 pred_offset, evx_block_desc *output_desc)
{
    evx_motion_selection selection;
    selection.best_x = pixel_x;
    selection.best_y = pixel_y;
    selection.best_sad = EVX_MAX_INT32;
    selection.best_mad = EVX_MAX_INT32;
    selection.best_ssd = EVX_MAX_INT32;
    selection.sp_amount = 0;
    selection.sp_index = 0;
    selection.sp_enabled = false;

    evx_prediction_params params;
    params.pixel_x = pixel_x;
    params.pixel_y = pixel_y;
    params.mad_skip_threshold = ((frame.quality >> 2) + 1);

    // Search for the closest match to our current source block within the prediction image.
    uint32 inter_pred_index = query_prediction_index_by_offset(frame, pred_offset);
    params.prediction = const_cast<image_set *>(&cache_bank->prediction_cache[inter_pred_index]);

    // Each block type incurs a different cost to encode. If we encounter a sad tie then
    // we select the block that has the lowest cost. We accomplish this by configuring our
    // initial best_sad to indicate the cheapest block type.
    macroblock test_block;
    create_macroblock(*params.prediction, pixel_x, pixel_y, &test_block);  
    selection.best_sad = compute_block_sad(src_block, test_block);  
    selection.best_mad = compute_block_mad(src_block, test_block);
     
    // If we've already found a suitable copy block then we avoid an exhaustive
    // motion search and simply encode as inter_copy.
    if (selection.best_mad >= params.mad_skip_threshold)
    {
        // Scan the following values around our pixel:
        //                                                           
        //      X          X          X                                                  
        //                                                        
        //      X        Pixel        X                                                   
        //                                                        
        //      X          X          X  

        for (int16 i = EVX_MOTION_SEARCH_RADIUS; i > 0; i >>= 1)
        {
            perform_inter_motion_search(-i, -i, i, i, i, params, src_block, &selection);
        }

        // perform subpixel motion estimation
        perform_inter_subpixel_motion_search(params, src_block, &cache_bank->motion_block, &selection);    
    }

    // Fill out our block descriptor using our closest match.
    clear_block_desc(output_desc);

    EVX_SET_INTRA_BLOCK_TYPE_BIT(output_desc->block_type, false);

    if (selection.best_x != pixel_x || selection.best_y != pixel_y || selection.sp_enabled)
    {
        EVX_SET_MOTION_BLOCK_TYPE_BIT(output_desc->block_type, true);
    }

    if (selection.best_mad < params.mad_skip_threshold)
    {
        EVX_SET_COPY_BLOCK_TYPE_BIT(output_desc->block_type, true);
    }

    output_desc->prediction_target = pred_offset;
    output_desc->motion_x = selection.best_x - pixel_x;
    output_desc->motion_y = selection.best_y - pixel_y;
    output_desc->sp_pred = selection.sp_enabled;
    output_desc->sp_amount = selection.sp_amount;
    output_desc->sp_index = selection.sp_index;

    return selection.best_sad;
}

} // namespace evx