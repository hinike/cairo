
/*
// Copyright (c) 2009-2014 Joe Bertolami. All Right Reserved.
//
// stream.h
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice, this
//     list of conditions and the following disclaimer.
//
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Additional Information:
//
//   For more information, visit http://www.bertolami.com.
*/

#ifndef __EVX_STREAM_H__
#define __EVX_STREAM_H__

#include "base.h"
#include "math.h"
#include "bitstream.h"
#include "abac.h"

namespace evx {

// These methods provide a limited range huffman precoder.
evx_status stream_encode_huffman_value(uint8 value, bit_stream *output);
evx_status stream_decode_huffman_value(bit_stream *data, uint8 *output);

evx_status stream_encode_huffman_values(uint8 *data, uint32 count, bit_stream *output);
evx_status stream_decode_huffman_values(bit_stream *data, uint32 count, uint8 *output);

// These methods provide a stream interface for our golomb precoder.
evx_status stream_encode_value(uint16 value, bit_stream *out_buffer);
evx_status stream_encode_value(int16 value, bit_stream *out_buffer);

evx_status stream_encode_values(uint16 *data, uint32 count, bit_stream *out_buffer);
evx_status stream_encode_values(int16 *data, uint32 count, bit_stream *out_buffer);

evx_status stream_decode_value(bit_stream *data, uint16 *output);
evx_status stream_decode_value(bit_stream *data, int16 *output);

evx_status stream_decode_values(bit_stream *data, uint32 count, uint16 *output);
evx_status stream_decode_values(bit_stream *data, uint32 count, int16 *output);

// These methods combine a golomb precoder with the entropy coder.
evx_status entropy_stream_encode_value(uint16 value, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output);
evx_status entropy_stream_encode_value(int16 value, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output);

evx_status entropy_stream_decode_value(bit_stream *input, entropy_coder *coder, bit_stream *feed_stream, uint16 *output);
evx_status entropy_stream_decode_value(bit_stream *input, entropy_coder *coder, bit_stream *feed_stream, int16 *output);

// Block based entropy encoding. we only support signed golomb codes because our 
// dct coefficients are signed 16-bit values.
// 
// Note: raw data buffers must not be padded (stride must equal width).
evx_status entropy_stream_encode_4x4(int16 *input, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output);
evx_status entropy_stream_encode_8x8(int16 *input, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output);
evx_status entropy_stream_encode_16x16(int16 *input, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output);

evx_status entropy_stream_decode_4x4(bit_stream *input, entropy_coder *coder, bit_stream *feed_stream, int16 *output);
evx_status entropy_stream_decode_8x8(bit_stream *input, entropy_coder *coder, bit_stream *feed_stream, int16 *output);
evx_status entropy_stream_decode_16x16(bit_stream *input, entropy_coder *coder, bit_stream *feed_stream, int16 *output);

// run length stream interfaces prefix the output with the number of non-zero coefficients in the block.
evx_status entropy_rle_stream_encode_8x8(int16 *input, bit_stream *feed_stream, entropy_coder *coder, bit_stream *output);
evx_status entropy_rle_stream_decode_8x8(bit_stream *input, entropy_coder *coder, bit_stream *feed_stream, int16 *output);

} // namespace evx

#endif // __EVX_STREAM_H__