/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "parser.h"
#include "mlp_parser.h"
#include "mlp.h"

typedef struct MLPParseContext
{
    ParseContext pc;

    int bytes_left;

    int in_sync;

    int num_substreams;
} MLPParseContext;

/** Number of bits used for VLC lookup - longest huffman code is 9. */
#define VLC_BITS            9

typedef struct SubStream {
    //! Set if a valid restart header has been read. Otherwise the substream cannot be decoded.
    uint8_t     restart_seen;

    //@{
    /** restart header data */
    //! The type of noise to be used in the rematrix stage.
    uint16_t    noise_type;

    //! The index of the first channel coded in this substream.
    uint8_t     min_channel;
    //! The index of the last channel coded in this substream.
    uint8_t     max_channel;
    //! The number of channels input into the rematrix stage.
    uint8_t     max_matrix_channel;
    //! For each channel output by the matrix, the output channel to map it to
		uint8_t     ch_assign[MAX_CHANNELS_ALL];

    //! The left shift applied to random noise in 0x31ea substreams.
    uint8_t     noise_shift;
    //! The current seed value for the pseudorandom noise generator(s).
    uint32_t    noisegen_seed;

    //! Set if the substream contains extra info to check the size of VLC blocks.
    uint8_t     data_check_present;

    //! Bitmask of which parameter sets are conveyed in a decoding parameter block.
    uint8_t     param_presence_flags;
#define PARAM_BLOCKSIZE     (1 << 7)
#define PARAM_MATRIX        (1 << 6)
#define PARAM_OUTSHIFT      (1 << 5)
#define PARAM_QUANTSTEP     (1 << 4)
#define PARAM_FIR           (1 << 3)
#define PARAM_IIR           (1 << 2)
#define PARAM_HUFFOFFSET    (1 << 1)
#define PARAM_PRESENCE      (1 << 0)
    //@}

    //@{
    /** matrix data */

    //! Number of matrices to be applied.
    uint8_t     num_primitive_matrices;

    //! matrix output channel
    uint8_t     matrix_out_ch[MAX_MATRICES];

    //! Whether the LSBs of the matrix output are encoded in the bitstream.
    uint8_t     lsb_bypass[MAX_MATRICES];
    //! Matrix coefficients, stored as 2.14 fixed point.
		int32_t     matrix_coeff[MAX_MATRICES][MAX_CHANNELS_ALL];
    //! Left shift to apply to noise values in 0x31eb substreams.
    uint8_t     matrix_noise_shift[MAX_MATRICES];
    //@}

    //! Left shift to apply to Huffman-decoded residuals.
		uint8_t     quant_step_size[MAX_CHANNELS_ALL];

    //! number of PCM samples in current audio block
    uint16_t    blocksize;
    //! Number of PCM samples decoded so far in this frame.
    uint16_t    blockpos;

    //! Left shift to apply to decoded PCM values to get final 24-bit output.
		int8_t      output_shift[MAX_CHANNELS_ALL];

    //! Running XOR of all output samples.
    int32_t     lossless_check_data;

} SubStream;

typedef struct MLPDecodeContext {
    AVCodecContext *avctx;

    //! Current access unit being read has a major sync.
    int         is_major_sync_unit;

    //! Set if a valid major sync block has been read. Otherwise no decoding is possible.
    uint8_t     params_valid;

    //! Number of substreams contained within this stream.
    uint8_t     num_substreams;

    //! Index of the last substream to decode - further substreams are skipped.
    uint8_t     max_decoded_substream;

    //! number of PCM samples contained in each frame
    int         access_unit_size;
    //! next power of two above the number of samples in each frame
    int         access_unit_size_pow2;

    SubStream   substream[MAX_SUBSTREAMS];

		ChannelParams channel_params[MAX_CHANNELS_ALL];

    int         matrix_changed;
		int         filter_changed[MAX_CHANNELS_ALL][NUM_FILTERS];

    int8_t      noise_buffer[MAX_BLOCKSIZE_POW2];
		int8_t      bypassed_lsbs[MAX_BLOCKSIZE][MAX_CHANNELS_ALL];
		int32_t     sample_buffer[MAX_BLOCKSIZE][MAX_CHANNELS_ALL];

    DSPContext  dsp;
} MLPDecodeContext;
