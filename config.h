
/*
// Copyright (c) 2009-2014 Joe Bertolami. All Right Reserved.
//
// config.h
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

#ifndef __EVX_CONFIG_H__
#define __EVX_CONFIG_H__

// Frame parameters

#define EVX_ALLOW_INTER_FRAMES                                      (1)
#define EVX_REFERENCE_FRAME_COUNT                                   (4)
#define EVX_DEFAULT_QUALITY_LEVEL                                   (8)
#define EVX_PERIODIC_INTRA_RATE                                     (3600)     // 0 implies only i-frames
#define EVX_ENABLE_CHROMA_SUPPORT                                   (1)        // 0 - grayscale, 1 - color

// Quantization parameters. Disabling quantization will enable a high
// quality semi-lossless mode.

#define EVX_QUANTIZATION_ENABLED                                    (1)
#define EVX_ENABLE_LINEAR_QUANTIZATION                              (0)        // 0 - MPEG, 1 - H.263
#define EVX_ROUNDED_QUANTIZATION                                    (1)      
#define EVX_ADAPTIVE_QUANTIZATION                                   (1)

// Deblocking parameters
#define EVX_ENABLE_DEBLOCKING                                       (1)

#endif // __EVX_CONFIG_H__
