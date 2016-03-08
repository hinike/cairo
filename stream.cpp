
#include "stream.h"
#include "golomb.h"
#include "scan.h"

namespace evx {

evx_status stream_encode_huffman_value(uint8 value, bit_stream *output)
{
    if (value >= 8)
    {
        // This simple precoder only supports values [0:7].
        return evx_post_error(EVX_ERROR_INVALIDARG);
    }

    uint8 bit = 1;
    uint8 count = 0;
    bit <<= value;

    while (bit) 
    {
        output->write_bit(bit & 0x1);
        bit >>= 0x1;
        count++;

        if (count >= 7) break;
    }

    return EVX_SUCCESS;
}

evx_status stream_decode_huffman_value(bit_stream *data, uint8 *output)
{
    uint8 value = 0;

    for (uint8 i = 0; i < 7; i++)
    {
        uint8 bit = 0;
        data->read_bit(&bit);

        if (bit) break;
        value++;
    }

    return value;
}

evx_status stream_encode_huffman_values(uint8 *data, uint32 count, bit_stream *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!data|| 0 == count || !output || 0 == output->query_capacity())
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    for (uint8 i = 0; i < count; i++)
    {
        if (evx_failed(stream_encode_huffman_value(data[i], output)))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    return EVX_SUCCESS;
}

evx_status stream_decode_huffman_values(bit_stream *data, uint32 count, uint8 *output)
{
    if (EVX_PARAM_CHECK)
    {
        if ( !data || 0 == data->query_occupancy() || !output || 0 == count)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    for (uint32 i = 0; i < count; ++i)
    {
        if (EVX_SUCCESS != stream_decode_huffman_value(data, output+i))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    return EVX_SUCCESS;
}

evx_status stream_encode_value(uint16 value, bit_stream *out_buffer)
{
    if (EVX_PARAM_CHECK)
    {
        if (!out_buffer|| 0 == out_buffer->query_capacity())
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    uint8 count = 0;
    uint32 golomb_value = encode_unsigned_golomb_value(value, &count);

    return out_buffer->write_bits(&golomb_value, count);
}

evx_status stream_encode_value(int16 value, bit_stream *out_buffer)
{
    if (EVX_PARAM_CHECK)
    {
        if (!out_buffer|| 0 == out_buffer->query_capacity())
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    uint8 count = 0;
    uint32 golomb_value = encode_signed_golomb_value(value, &count);

    return out_buffer->write_bits(&golomb_value, count);
}

evx_status stream_encode_values(uint16 *data, uint32 count, bit_stream *out_buffer)
{
    if (EVX_PARAM_CHECK)
    {
        if (!data|| 0 == count || !out_buffer || 0 == out_buffer->query_capacity())
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    for (uint32 i = 0; i < count; ++i)
    {
        if (EVX_SUCCESS != stream_encode_value(data[i], out_buffer))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    return EVX_SUCCESS;
}

evx_status stream_encode_values(int16 *data, uint32 count, bit_stream *out_buffer)
{
    if (EVX_PARAM_CHECK)
    {
        if (!data|| 0 == count || !out_buffer || 0 == out_buffer->query_capacity())
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    for (uint32 i = 0; i < count; ++i)
    {
        if (EVX_SUCCESS != stream_encode_value(data[i], out_buffer))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    return EVX_SUCCESS;
}

evx_status stream_decode_value(bit_stream *data, uint16 *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!data || 0 == data->query_occupancy() || !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    uint32 value = 0;
    uint8 count = evx_min2(32, data->query_byte_occupancy());

    evx_status result = data->peek_bits(&value, count);
    *output = decode_unsigned_golomb_value(value, &count);
    data->seek(count);

    return result;
}

evx_status stream_decode_value(bit_stream *data, int16 *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!data || 0 == data->query_occupancy() || !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    uint32 value = 0;
    uint8 count = evx_min2(32, data->query_byte_occupancy());
    
    evx_status result = data->peek_bits(&value, count);
    *output = decode_signed_golomb_value(value, &count);
    data->seek(count);

    return result;
}

evx_status stream_decode_values(bit_stream *data, uint32 count, uint16 *output)
{
    if (EVX_PARAM_CHECK)
    {
        if ( !data || 0 == data->query_occupancy() || !output || 0 == count)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    for (uint32 i = 0; i < count; ++i)
    {
        if (EVX_SUCCESS != stream_decode_value(data, output+i))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    return EVX_SUCCESS;
}

evx_status stream_decode_values(bit_stream *data, uint32 count, int16 *output)
{
    if (EVX_PARAM_CHECK)
    {
        if ( !data || 0 == data->query_occupancy() || !output || 0 == count)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    for (uint32 i = 0; i < count; ++i)
    {
        if (EVX_SUCCESS != stream_decode_value(data, output+i))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    return EVX_SUCCESS;
}

evx_status entropy_stream_encode_value(uint16 value, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!feed_stream || !coder || !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    if (EVX_SUCCESS != stream_encode_value(value, feed_stream))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (EVX_SUCCESS != coder->encode(feed_stream, output, false))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    return EVX_SUCCESS;
}

evx_status entropy_stream_encode_value(int16 value, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!feed_stream || !coder || !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    if (EVX_SUCCESS != stream_encode_value(value, feed_stream))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (EVX_SUCCESS != coder->encode(feed_stream, output, false))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    return EVX_SUCCESS;
}

evx_status entropy_stream_decode_value(bit_stream *input, entropy_coder *coder, bit_stream *feed_stream, uint16 *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!input || !coder || !feed_stream|| !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    uint16 result = 0; 
    uint8 zero_count = 0;
    uint8 bit_count = 0;
    uint8 bit_value = 0;

    if (EVX_SUCCESS != coder->decode(1, input, feed_stream, false))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (EVX_SUCCESS != feed_stream->read_bit(&bit_value))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    while (!bit_value) 
    {
        zero_count++;

        if (EVX_SUCCESS != coder->decode(1, input, feed_stream, false))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }

        if (EVX_SUCCESS != feed_stream->read_bit(&bit_value))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    bit_count = zero_count + 1;

    for (uint8 i = 0; i < bit_count; i++) 
    {
        result <<= 1;
        result |= bit_value & 0x1;

        if (i < bit_count - 1)
        {
            if (EVX_SUCCESS != coder->decode(1, input, feed_stream, false))
            {
                return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
            }

            if (EVX_SUCCESS != feed_stream->read_bit(&bit_value))
            {
                return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
            }
        }
    }

    result -= 1;
    *output = result;

    return EVX_SUCCESS;
}

evx_status entropy_stream_decode_value(bit_stream *input, entropy_coder *coder, bit_stream *feed_stream, int16 *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!input || !coder || !feed_stream|| !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    int16 result = 0;
    uint8 zero_count = 0;
    uint8 bit_count = 0;
    uint8 bit_value = 0;
    int16 sign = 0;

    if (EVX_SUCCESS != coder->decode(1, input, feed_stream, false))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    if (EVX_SUCCESS != feed_stream->read_bit(&bit_value))
    {
        return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
    }

    while (!bit_value) 
    {
        zero_count++;
        
        if (EVX_SUCCESS != coder->decode(1, input, feed_stream, false))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }

        if (EVX_SUCCESS != feed_stream->read_bit(&bit_value))
        {
            return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
        }
    }

    bit_count = zero_count + 1;

    for (uint8 i = 0; i < bit_count; i++) 
    {
        result <<= 1;
        result |= bit_value & 0x1;

        if (i < bit_count - 1)
        {
            if (EVX_SUCCESS != coder->decode(1, input, feed_stream, false))
            {
                return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
            }

            if (EVX_SUCCESS != feed_stream->read_bit(&bit_value))
            {
                return evx_post_error(EVX_ERROR_EXECUTION_FAILURE);
            }
        }
    }

    // Remove the lowest bit as our sign bit.
    sign = 1 - 2 * (result & 0x1);
    result = sign * ((result >> 1) & 0x7FFF);

    // Defend against overflow on min int16.
    bit_count += zero_count;

    if (bit_count > 0x20) 
    {
        result |= 0x8000;
    }

    *output = result;

    return EVX_SUCCESS;
}

evx_status entropy_stream_encode_4x4(int16 *input, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!input || !feed_stream || !coder || !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    for (uint32 read_index = 0; read_index < 16; ++read_index)
    {
        // It's very important that we encode these as signed integers. { -2, -1, 0, 1, 2 }
        // are highly likely values in a prediction block, so we encode them with a signed
        // exp-golomb coder.

        entropy_stream_encode_value(input[EVX_MACROBLOCK_4x4_ZIGZAG[read_index]], feed_stream, coder, output);
    }

    return EVX_SUCCESS;
}

evx_status entropy_stream_encode_8x8(int16 *input, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!input || !feed_stream || !coder || !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    for (uint32 read_index = 0; read_index < 64; ++read_index)
    {
        entropy_stream_encode_value(input[EVX_MACROBLOCK_8x8_ZIGZAG[read_index]], feed_stream, coder, output);
    }

    return EVX_SUCCESS;
}

evx_status entropy_stream_encode_16x16(int16 *input, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!input || !feed_stream || !coder || !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    for (uint32 read_index = 0; read_index < 256; ++read_index)
    {
        entropy_stream_encode_value(input[EVX_MACROBLOCK_16x16_ZIGZAG[read_index]], feed_stream, coder, output);
    }

    return EVX_SUCCESS;
}

evx_status entropy_stream_decode_4x4(bit_stream *input, entropy_coder *coder, bit_stream *feed_stream, int16 *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!input || !coder || !feed_stream || !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    for (uint32 read_index = 0; read_index < 16; ++read_index)
    {
        entropy_stream_decode_value(input, coder, feed_stream, &(output[EVX_MACROBLOCK_4x4_ZIGZAG[read_index]]));
    }

    return EVX_SUCCESS;
}

evx_status entropy_stream_decode_8x8(bit_stream *input, entropy_coder *coder, bit_stream *feed_stream, int16 *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!input || !coder || !feed_stream || !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    for (uint32 read_index = 0; read_index < 64; ++read_index)
    {
        entropy_stream_decode_value(input, coder, feed_stream, &(output[EVX_MACROBLOCK_8x8_ZIGZAG[read_index]]));
    }

    return EVX_SUCCESS;
}

evx_status entropy_stream_decode_16x16(bit_stream *input, entropy_coder *coder, bit_stream *feed_stream, int16 *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!input || !coder || !feed_stream || !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    for (uint32 read_index = 0; read_index < 256; ++read_index)
    {
        entropy_stream_decode_value(input, coder, feed_stream, &(output[EVX_MACROBLOCK_16x16_ZIGZAG[read_index]]));
    }

    return EVX_SUCCESS;
}

evx_status entropy_rle_stream_encode_8x8(int16 *input, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!input || !feed_stream || !coder || !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    int32 run_length = 63;

    // Determine our run-length and code it as a golomb entry.
    for (run_length = 63; run_length >= 0; --run_length)
    {
        if (input[EVX_MACROBLOCK_8x8_ZIGZAG[run_length]])
        {
            break;
        }
    }

    run_length = (run_length + 1);

    entropy_stream_encode_value((uint16) run_length, feed_stream, coder, output);

    for (uint32 read_index = 0; read_index < run_length; ++read_index)
    {
        entropy_stream_encode_value(input[EVX_MACROBLOCK_8x8_ZIGZAG[read_index]], feed_stream, coder, output);
    }

    return EVX_SUCCESS;
}

evx_status entropy_rle_stream_decode_8x8(bit_stream *input, entropy_coder *coder, bit_stream *feed_stream, int16 *output)
{
    if (EVX_PARAM_CHECK)
    {
        if (!input || !coder || !feed_stream || !output)
        {
            return evx_post_error(EVX_ERROR_INVALIDARG);
        }
    }

    uint16 run_length = 0;

    memset(output, 0, sizeof(int16) * 64);

    entropy_stream_decode_value(input, coder, feed_stream, &run_length);

    for (uint32 read_index = 0; read_index < run_length; ++read_index)
    {
        entropy_stream_decode_value(input, coder, feed_stream, &(output[EVX_MACROBLOCK_8x8_ZIGZAG[read_index]]));
    }

    return EVX_SUCCESS;
}

} // namespace evx