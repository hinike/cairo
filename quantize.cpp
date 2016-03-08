
#include "quantize.h"
#include "analysis.h"

// The quantizer scale factor enables us to adjust the scale of the 
// quantization matrix. The default value is 16, which offers a reasonable
// tradeoff between quality and efficiency for most content.

#define EVX_QUANTIZER_SCALE_FACTOR                      (16)

namespace evx {

const int16 default_intra_8x8_qm[] = 
{
     8, 17, 18, 19, 21, 23, 25, 27,
    17, 18, 19, 21, 23, 25, 27, 28,
    20, 21, 22, 23, 24, 26, 28, 30,
    21, 22, 23, 24, 26, 28, 30, 32,
    22, 23, 24, 26, 28, 30, 32, 35,
    23, 24, 26, 28, 30, 32, 35, 38,
    25, 26, 28, 30, 32, 35, 38, 41,
    27, 28, 30, 32, 35, 38, 41, 45
};

const int16 default_inter_8x8_qm[] = 
{
    16, 17, 18, 19, 20, 21, 22, 23,
    17, 18, 19, 20, 21, 22, 23, 24,
    18, 19, 20, 21, 22, 23, 24, 25,
    19, 20, 21, 22, 23, 24, 26, 27,
    20, 21, 22, 23, 25, 26, 27, 28,
    21, 22, 23, 24, 26, 27, 28, 30,
    22, 23, 24, 26, 27, 28, 30, 31,
    23, 24, 25, 27, 28, 30, 31, 33
};

int16 compute_luma_dc_scale(int16 qp)
{
    if (qp < 5)
        return 8;
    else if (qp < 9)
        return qp << 1;
    else if (qp < 25)
        return qp + 8;
    return (qp << 1) - 16;
}

int16 compute_chroma_dc_scale(int16 qp)
{
    if (qp < 5)
        return 8;
    else if (qp < 25)
        return (qp + 13) >> 1;
    return qp - 6;
}

// Adaptive quantization allows us to dynamically scale the quantization 
// parameter based on the statistical characteristics of the incoming block.

uint8 query_block_quantization_parameter(uint8 quality, const macroblock &src, EVX_BLOCK_TYPE block_type)
{   
#if EVX_QUANTIZATION_ENABLED
  #if EVX_ADAPTIVE_QUANTIZATION
    uint32 variance = compute_block_variance2(src);
    uint8 index = clip_range(log2(variance) >> 1, 1, EVX_MAX_MPEG_QUANT_LEVELS - 1);

    if (index > quality) return clip_range(quality + ((index - quality) >> 1), 1, EVX_MAX_MPEG_QUANT_LEVELS - 1);
    if (index < quality) return clip_range(quality - ((quality - index) >> 1), 1, EVX_MAX_MPEG_QUANT_LEVELS - 1);

    return quality;
  #else
    return quality;
  #endif
#else
    return 0;
#endif
}

void quantize_luma_intra_block_8x8(uint8 qp, int16 *source, int32 source_stride, int16 *dest, int32 dest_stride)
{
    // Quantize our luminance values.
    for (uint32 j = 0; j < 8; ++j)
    for (uint32 k = 0; k < 8; ++k)
    {   
        int16 qm_value = default_intra_8x8_qm[k + j * 8];
        int16 source_luma = source[k + j * source_stride];

#if EVX_ROUNDED_QUANTIZATION
        dest[k + j * dest_stride] = rounded_div(rounded_div(source_luma * EVX_QUANTIZER_SCALE_FACTOR, qm_value), qp << 1);
#else
        dest[k + j * dest_stride] = ((source_luma * EVX_QUANTIZER_SCALE_FACTOR) / qm_value) / (qp << 1);
#endif
    }

    // For intra matrices we weight the dc coefficient separately.
    int16 luma_dc_scale = compute_luma_dc_scale(qp);

#if EVX_ROUNDED_QUANTIZATION
    dest[0] = rounded_div(source[0], luma_dc_scale);
#else
    dest[0] = (source[0] / luma_dc_scale);
#endif
}

void quantize_chroma_intra_block_8x8(uint8 qp, int16 *source, int32 source_stride, int16 *dest, int32 dest_stride)
{
    // Quantize our luminance values.
    for (uint32 j = 0; j < 8; ++j)
    for (uint32 k = 0; k < 8; ++k)
    {   
        int16 qm_value = default_intra_8x8_qm[k + j * 8];
        int16 source_chroma = source[k + j * source_stride];

#if EVX_ROUNDED_QUANTIZATION
        dest[k + j * dest_stride] = rounded_div(rounded_div(source_chroma * EVX_QUANTIZER_SCALE_FACTOR, qm_value), qp << 1);
#else
        dest[k + j * dest_stride] = ((source_chroma * EVX_QUANTIZER_SCALE_FACTOR) / qm_value) / (qp << 1);
#endif
    }

    // For intra matrices we weight the dc coefficient separately.
    int16 chroma_dc_scale = compute_chroma_dc_scale(qp);

#if EVX_ROUNDED_QUANTIZATION
    dest[0] = rounded_div(source[0], chroma_dc_scale);
#else
    dest[0] = (source[0] / chroma_dc_scale);
#endif
}

void quantize_intra_block_linear_8x8(uint8 qp, int16 *source, int32 source_stride, int16 *dest, int32 dest_stride)
{
    for (uint32 j = 0; j < 8; ++j)
    for (uint32 k = 0; k < 8; ++k)
    {   
        int16 source_value = source[k + j * source_stride];

#if EVX_ROUNDED_QUANTIZATION
        dest[k + j * dest_stride] = rounded_div(source_value, qp << 1);
#else
        dest[k + j * dest_stride] = (source_value) / (qp << 1);
#endif
    }
}

void quantize_inter_block_8x8(uint8 qp, int16 *source, int32 source_stride, int16 *dest, int32 dest_stride)
{
    // Quantize our inter values.
    for (uint32 j = 0; j < 8; ++j)
    for (uint32 k = 0; k < 8; ++k)
    {   
        int16 qm_value = default_inter_8x8_qm[k + j * 8];
        int16 source_value = source[k + j * source_stride];

#if EVX_ROUNDED_QUANTIZATION
        int16 qfactor = rounded_div(source_value * EVX_QUANTIZER_SCALE_FACTOR, qm_value);
        dest[k + j * dest_stride] = rounded_div(qfactor - sign(qfactor) * qp, qp << 1);
#else
        int16 qfactor = (source_value * EVX_QUANTIZER_SCALE_FACTOR) / qm_value;
        dest[k + j * dest_stride] = (qfactor - sign(qfactor) * qp) / (qp << 1);
#endif
    }
}

void quantize_inter_block_linear_8x8(uint8 qp, int16 *source, int32 source_stride, int16 *dest, int32 dest_stride)
{
    for (uint32 j = 0; j < 8; ++j)
    for (uint32 k = 0; k < 8; ++k)
    {   
        int16 source_value = source[k + j * source_stride];
        int16 qm_value = (abs(source_value) - (qp >> 1));

#if EVX_ROUNDED_QUANTIZATION
        dest[k + j * dest_stride] = rounded_div(qm_value, qp << 1);
#else
        dest[k + j * dest_stride] = (qm_value) / (qp << 1);
#endif
        dest[k + j * dest_stride] *= sign(source_value);
    }
}

void inverse_quantize_luma_intra_block_8x8(uint8 qp, int16 *source, int32 source_stride, int16 *dest, int32 dest_stride)
{
    // Inverse quantize our luminance values.
    for (uint32 j = 0; j < 8; ++j)
    for (uint32 k = 0; k < 8; ++k)
    {   
        int16 qm_value = default_intra_8x8_qm[k + j * 8];
        int16 source_luma = source[k + j * source_stride];
        dest[k + j * dest_stride] = (2 * source_luma * qm_value * qp) / EVX_QUANTIZER_SCALE_FACTOR;
    }

    // For intra matrices we weight the dc coefficient separately.
    int16 luma_dc_scale = compute_luma_dc_scale(qp);
    dest[0] = source[0] * luma_dc_scale;
}

void inverse_quantize_chroma_intra_block_8x8(uint8 qp, int16 *source, int32 source_stride, int16 *dest, int32 dest_stride)
{
    // Inverse quantize our chrominance values.
    for (uint32 j = 0; j < 8; ++j)
    for (uint32 k = 0; k < 8; ++k)
    {   
        int16 qm_value = default_intra_8x8_qm[k + j * 8];
        int16 source_chroma = source[k + j * source_stride];
        dest[k + j * dest_stride] = (2 * source_chroma * qm_value * qp) / EVX_QUANTIZER_SCALE_FACTOR;
    }

    // For intra matrices we weight the dc coefficient separately.
    int16 chroma_dc_scale = compute_chroma_dc_scale(qp);
    dest[0] = source[0] * chroma_dc_scale;
}

void inverse_quantize_block_linear_8x8(uint8 qp, int16 *source, int32 source_stride, int16 *dest, int32 dest_stride)
{
    for (uint32 j = 0; j < 8; ++j)
    for (uint32 k = 0; k < 8; ++k)
    {   
        int16 source_value = source[k + j * source_stride];
        dest[k + j * dest_stride] = 0;

        if (source_value)
        {
            int16 mod_qp = (qp + 1) % 2;
            int16 qm_value = (abs(source_value) << 1) + 1;
            
            dest[k + j * dest_stride] = qm_value * qp - 1 * mod_qp;
            dest[k + j * dest_stride] *= sign(source_value);
        }
    }
}

void inverse_quantize_inter_block_8x8(uint8 qp, int16 *source, int32 source_stride, int16 *dest, int32 dest_stride)
{
    // Inverse quantize our inter values.
    for (uint32 j = 0; j < 8; ++j)
    for (uint32 k = 0; k < 8; ++k)
    {   
        int16 qm_value = default_inter_8x8_qm[k + j * 8];
        int16 source_value = source[k + j * source_stride];
        dest[k + j * dest_stride] = ((2 * source_value) * qm_value * qp) / EVX_QUANTIZER_SCALE_FACTOR; 
    }
}

void inverse_quantize_block_flat_8x8(uint8 qp, int16 *source, int32 source_stride, int16 *dest, int32 dest_stride)
{
    // Inverse quantize our inter values.
    for (uint32 j = 0; j < 8; ++j)
    for (uint32 k = 0; k < 8; ++k)
    {   
        int16 source_value = source[k + j * source_stride];
        dest[k + j * dest_stride] = source_value * qp; 
    }
}

void quantize_intra_macroblock(uint8 qp, const macroblock &source, macroblock *dest)
{
#if EVX_ENABLE_LINEAR_QUANTIZATION
    // Luminance blocks.
    quantize_intra_block_linear_8x8(qp, source.data_y, source.stride, dest->data_y, dest->stride);
    quantize_intra_block_linear_8x8(qp, source.data_y + 8, source.stride, dest->data_y + 8, dest->stride);
    quantize_intra_block_linear_8x8(qp, source.data_y + 8 * source.stride, source.stride, dest->data_y + 8 * dest->stride, dest->stride);
    quantize_intra_block_linear_8x8(qp, source.data_y + 8 * source.stride + 8, source.stride, dest->data_y + 8 * dest->stride + 8, dest->stride);

    // Chroma blocks.
    quantize_intra_block_linear_8x8(qp, source.data_u, source.stride >> 1, dest->data_u, dest->stride >> 1);
    quantize_intra_block_linear_8x8(qp, source.data_v, source.stride >> 1, dest->data_v, dest->stride >> 1);
#else
    // Luminance blocks.
    quantize_luma_intra_block_8x8(qp, source.data_y, source.stride, dest->data_y, dest->stride);
    quantize_luma_intra_block_8x8(qp, source.data_y + 8, source.stride, dest->data_y + 8, dest->stride);
    quantize_luma_intra_block_8x8(qp, source.data_y + 8 * source.stride, source.stride, dest->data_y + 8 * dest->stride, dest->stride);
    quantize_luma_intra_block_8x8(qp, source.data_y + 8 * source.stride + 8, source.stride, dest->data_y + 8 * dest->stride + 8, dest->stride);

    // Chroma blocks.
    quantize_chroma_intra_block_8x8(qp, source.data_u, source.stride >> 1, dest->data_u, dest->stride >> 1);
    quantize_chroma_intra_block_8x8(qp, source.data_v, source.stride >> 1, dest->data_v, dest->stride >> 1);
#endif
}

void quantize_inter_macroblock(uint8 qp, const macroblock &source, macroblock *dest)
{
#if EVX_ENABLE_LINEAR_QUANTIZATION
    // Luminance blocks.
    quantize_inter_block_linear_8x8(qp, source.data_y, source.stride, dest->data_y, dest->stride);
    quantize_inter_block_linear_8x8(qp, source.data_y + 8, source.stride, dest->data_y + 8, dest->stride);
    quantize_inter_block_linear_8x8(qp, source.data_y + 8 * source.stride, source.stride, dest->data_y + 8 * dest->stride, dest->stride);
    quantize_inter_block_linear_8x8(qp, source.data_y + 8 * source.stride + 8, source.stride, dest->data_y + 8 * dest->stride + 8, dest->stride);

    // Chroma blocks.
    quantize_inter_block_linear_8x8(qp, source.data_u, source.stride >> 1, dest->data_u, dest->stride >> 1);
    quantize_inter_block_linear_8x8(qp, source.data_v, source.stride >> 1, dest->data_v, dest->stride >> 1);
#else
    // Luminance blocks.
    quantize_inter_block_8x8(qp, source.data_y, source.stride, dest->data_y, dest->stride);
    quantize_inter_block_8x8(qp, source.data_y + 8, source.stride, dest->data_y + 8, dest->stride);
    quantize_inter_block_8x8(qp, source.data_y + 8 * source.stride, source.stride, dest->data_y + 8 * dest->stride, dest->stride);
    quantize_inter_block_8x8(qp, source.data_y + 8 * source.stride + 8, source.stride, dest->data_y + 8 * dest->stride + 8, dest->stride);

    // Chroma blocks.
    quantize_inter_block_8x8(qp, source.data_u, source.stride >> 1, dest->data_u, dest->stride >> 1);
    quantize_inter_block_8x8(qp, source.data_v, source.stride >> 1, dest->data_v, dest->stride >> 1);
#endif
}   

void inverse_quantize_intra_macroblock(uint8 qp, const macroblock &source, macroblock *dest)
{
#if EVX_ENABLE_LINEAR_QUANTIZATION
    // Luminance blocks.
    inverse_quantize_block_linear_8x8(qp, source.data_y, source.stride, dest->data_y, dest->stride);
    inverse_quantize_block_linear_8x8(qp, source.data_y + 8, source.stride, dest->data_y + 8, dest->stride);
    inverse_quantize_block_linear_8x8(qp, source.data_y + 8 * source.stride, source.stride, dest->data_y + 8 * dest->stride, dest->stride);
    inverse_quantize_block_linear_8x8(qp, source.data_y + 8 * source.stride + 8, source.stride, dest->data_y + 8 * dest->stride + 8, dest->stride);

    // Chroma blocks.
    inverse_quantize_block_linear_8x8(qp, source.data_u, source.stride >> 1, dest->data_u, dest->stride >> 1);
    inverse_quantize_block_linear_8x8(qp, source.data_v, source.stride >> 1, dest->data_v, dest->stride >> 1);
#else
    // Luminance blocks.
    inverse_quantize_luma_intra_block_8x8(qp, source.data_y, source.stride, dest->data_y, dest->stride);
    inverse_quantize_luma_intra_block_8x8(qp, source.data_y + 8, source.stride, dest->data_y + 8, dest->stride);
    inverse_quantize_luma_intra_block_8x8(qp, source.data_y + 8 * source.stride, source.stride, dest->data_y + 8 * dest->stride, dest->stride);
    inverse_quantize_luma_intra_block_8x8(qp, source.data_y + 8 * source.stride + 8, source.stride, dest->data_y + 8 * dest->stride + 8, dest->stride);

    // Chroma blocks.
    inverse_quantize_chroma_intra_block_8x8(qp, source.data_u, source.stride >> 1, dest->data_u, dest->stride >> 1);
    inverse_quantize_chroma_intra_block_8x8(qp, source.data_v, source.stride >> 1, dest->data_v, dest->stride >> 1);
#endif
}

void inverse_quantize_inter_macroblock(uint8 qp, const macroblock &source, macroblock *dest)
{
#if EVX_ENABLE_LINEAR_QUANTIZATION
    // Luminance blocks.
    inverse_quantize_block_linear_8x8(qp, source.data_y, source.stride, dest->data_y, dest->stride);
    inverse_quantize_block_linear_8x8(qp, source.data_y + 8, source.stride, dest->data_y + 8, dest->stride);
    inverse_quantize_block_linear_8x8(qp, source.data_y + 8 * source.stride, source.stride, dest->data_y + 8 * dest->stride, dest->stride);
    inverse_quantize_block_linear_8x8(qp, source.data_y + 8 * source.stride + 8, source.stride, dest->data_y + 8 * dest->stride + 8, dest->stride);

    // Chroma blocks.
    inverse_quantize_block_linear_8x8(qp, source.data_u, source.stride >> 1, dest->data_u, dest->stride >> 1);
    inverse_quantize_block_linear_8x8(qp, source.data_v, source.stride >> 1, dest->data_v, dest->stride >> 1);
#else
    // Luminance blocks.
    inverse_quantize_inter_block_8x8(qp, source.data_y, source.stride, dest->data_y, dest->stride);
    inverse_quantize_inter_block_8x8(qp, source.data_y + 8, source.stride, dest->data_y + 8, dest->stride);
    inverse_quantize_inter_block_8x8(qp, source.data_y + 8 * source.stride, source.stride, dest->data_y + 8 * dest->stride, dest->stride);
    inverse_quantize_inter_block_8x8(qp, source.data_y + 8 * source.stride + 8, source.stride, dest->data_y + 8 * dest->stride + 8, dest->stride);

    // Chroma blocks.
    inverse_quantize_inter_block_8x8(qp, source.data_u, source.stride >> 1, dest->data_u, dest->stride >> 1);
    inverse_quantize_inter_block_8x8(qp, source.data_v, source.stride >> 1, dest->data_v, dest->stride >> 1);
#endif
}

// Modified MPEG-2 quantization with adaptive qp.
void quantize_macroblock(uint8 qp, EVX_BLOCK_TYPE block_type, const macroblock &source, macroblock *__restrict dest)
{
#if EVX_QUANTIZATION_ENABLED
    if (EVX_IS_INTRA_BLOCK_TYPE(block_type) && !EVX_IS_MOTION_BLOCK_TYPE(block_type))
        return quantize_intra_macroblock(qp, source, dest);
    else
        return quantize_inter_macroblock(qp, source, dest);
#else
    copy_macroblock(source, dest);
#endif
}

void inverse_quantize_macroblock(uint8 qp, EVX_BLOCK_TYPE block_type, const macroblock &source, macroblock *__restrict dest)
{
#if EVX_QUANTIZATION_ENABLED
    if (EVX_IS_INTRA_BLOCK_TYPE(block_type) && !EVX_IS_MOTION_BLOCK_TYPE(block_type))
        return inverse_quantize_intra_macroblock(qp, source, dest);
    else
        return inverse_quantize_inter_macroblock(qp, source, dest);
#else
    copy_macroblock(source, dest);
#endif
}

} // namespace evx