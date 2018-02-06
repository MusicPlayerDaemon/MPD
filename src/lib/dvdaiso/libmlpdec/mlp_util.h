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

#ifndef MLP_UTIL_H
#define MLP_UTIL_H

#include <assert.h>
#include <malloc.h>
#include <memory.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define inline __inline
#define ALT_BITSTREAM_READER
#define CONFIG_AUDIO_NONSHORT

/*****************************************************************************/
/***   bswap.h                                                             ***/
/*****************************************************************************/

static inline uint16_t bswap_16(uint16_t x) {
  x= (x>>8) | (x<<8);
  return x;
}

static inline uint32_t bswap_32(uint32_t x) {
  x= ((x<<8)&0xFF00FF00) | ((x>>8)&0x00FF00FF);
  x= (x>>16) | (x<<16);
  return x;
}

static inline uint64_t bswap_64(uint64_t x) {
  union {
    uint64_t ll;
    uint32_t l[2];
  } w, r;
  w.ll = x;
  r.l[0] = bswap_32 (w.l[1]);
  r.l[1] = bswap_32 (w.l[0]);
  return r.ll;
}

// be2me ... BigEndian to MachineEndian
// le2me ... LittleEndian to MachineEndian

#if HAVE_BIGENDIAN
#define be2me_16(x) (x)
#define be2me_32(x) (x)
#define be2me_64(x) (x)
#define le2me_16(x) bswap_16(x)
#define le2me_32(x) bswap_32(x)
#define le2me_64(x) bswap_64(x)
#else
#define be2me_16(x) bswap_16(x)
#define be2me_32(x) bswap_32(x)
#define be2me_64(x) bswap_64(x)
#define le2me_16(x) (x)
#define le2me_32(x) (x)
#define le2me_64(x) (x)
#endif

/*****************************************************************************/
/***   common.h                                                            ***/
/*****************************************************************************/

#define FFMAX(a, b) ((a) > (b) ? (a) : (b))
#define FFMIN(a, b) ((a) > (b) ? (b) : (a))
#define av_cold

/*****************************************************************************/
/***   intreadwrite.h                                                      ***/
/*****************************************************************************/

#define AV_RB16(x) ((((const uint8_t*)(x))[0] << 8) | ((const uint8_t*)(x))[1])
#define AV_RL16(x) ((((const uint8_t*)(x))[1] << 8) | ((const uint8_t*)(x))[0])
#define AV_RB32(x) ((((const uint8_t*)(x))[0] << 24) | (((const uint8_t*)(x))[1] << 16) | (((const uint8_t*)(x))[2] << 8) | ((const uint8_t*)(x))[3])
#define AV_RL32(x) ((((const uint8_t*)(x))[3] << 24) | (((const uint8_t*)(x))[2] << 16) | (((const uint8_t*)(x))[1] << 8) | ((const uint8_t*)(x))[0])

/*****************************************************************************/
/***   mem.h                                                               ***/
/*****************************************************************************/

/**
 * Allocate or reallocate a block of memory.
 * If \p ptr is NULL and \p size > 0, allocate a new block. If \p
 * size is zero, free the memory block pointed by \p ptr.
 * @param size Size in bytes for the memory block to be allocated or
 * reallocated.
 * @param ptr Pointer to a memory block already allocated with
 * av_malloc(z)() or av_realloc() or NULL.
 * @return Pointer to a newly reallocated block or NULL if it cannot
 * reallocate or the function is used to free the memory block.
 * @see av_fast_realloc()
 */
void* av_realloc(void* ptr, unsigned int size);

/*****************************************************************************/
/***   avcodec.h                                                           ***/
/*****************************************************************************/

enum CodecID {
	CODEC_ID_NONE,
	CODEC_ID_MLP,
	CODEC_ID_TRUEHD
};

enum CodecType {
	CODEC_TYPE_UNKNOWN = -1,
	CODEC_TYPE_VIDEO,
	CODEC_TYPE_AUDIO,
	CODEC_TYPE_DATA,
	CODEC_TYPE_SUBTITLE,
	CODEC_TYPE_ATTACHMENT,
	CODEC_TYPE_NB
};

/**
 * all in native-endian format
 */
enum _SampleFormat {
	SAMPLE_FMT_NONE = -1,
	SAMPLE_FMT_U8,              ///< unsigned 8 bits
	SAMPLE_FMT_S16,             ///< signed 16 bits
	SAMPLE_FMT_S32,             ///< signed 32 bits
	SAMPLE_FMT_FLT,             ///< float
	SAMPLE_FMT_DBL,             ///< double
	SAMPLE_FMT_NB               ///< Number of sample formats. DO NOT USE if dynamically linking to libavcodec
};

/* in bytes */
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

/**
 * Required number of additionally allocated bytes at the end of the input bitstream for decoding.
 * This is mainly needed because some optimized bitstream readers read
 * 32 or 64 bit at once and could read over the end.<br>
 * Note: If the first 23 bits of the additional bytes are not 0, then damaged
 * MPEG bitstreams could cause overread and segfault.
 */
#define FF_INPUT_BUFFER_PADDING_SIZE 8

/**
 * main external API structure.
 * New fields can be added to the end with minor version bumps.
 * Removal, reordering and changes to existing fields require a major
 * version bump.
 * sizeof(AVCodecContext) must not be used outside libav*.
 */
typedef struct AVCodecContext {
	/**
	 * the average bitrate
	 * - encoding: Set by user; unused for constant quantizer encoding.
	 * - decoding: Set by libavcodec. 0 or some bitrate if this info is available in the stream.
	 */
	int bit_rate;
	int sample_rate; // samples per second
	int channels;
	/**
	 * audio sample format
	 * - encoding: Set by user.
	 * - decoding: Set by libavcodec.
	 */
	enum _SampleFormat sample_fmt;
	/**
	 * bits per sample/pixel from the demuxer (needed for huffyuv).
	 * - encoding: Set by libavcodec.
	 * - decoding: Set by user.
	 */
	int bits_per_sample;
	/**
	 * Bits per sample/pixel of internal libavcodec pixel/sample format.
	 * This field is applicable only when sample_fmt is SAMPLE_FMT_S32.
	 * - encoding: set by user.
	 * - decoding: set by libavcodec.
	 */
	int bits_per_raw_sample;
	int frame_size;
	void* priv_data;
	/**
	 * Decoder should decode to this many channels if it can (0 for default)
	 * - encoding: unused
	 * - decoding: Set by user.
	 */
	int request_channels;
	enum CodecID codec_id; /* see CODEC_ID_xxx */
 } AVCodecContext;

/* frame parsing */
typedef struct AVCodecParserContext {
	void* priv_data;
	struct AVCodecParser* parser;
	int64_t frame_offset;      /* offset of the current frame */
	int64_t cur_offset;        /* current offset (incremented by each av_parser_parse()) */
	int64_t last_frame_offset; /* offset of the last frame */
	/* video info */
	int pict_type;   /* XXX: Put it back in AVCodecContext. */
	int repeat_pict; /* XXX: Put it back in AVCodecContext. */
	int64_t pts;     /* pts of the current frame */
	int64_t dts;     /* dts of the current frame */
	/* private data */
	int64_t last_pts;
	int64_t last_dts;
	int fetch_timestamp;
#define AV_PARSER_PTS_NB 4
	int cur_frame_start_index;
	int64_t cur_frame_offset[AV_PARSER_PTS_NB];
	int64_t cur_frame_pts[AV_PARSER_PTS_NB];
	int64_t cur_frame_dts[AV_PARSER_PTS_NB];
	int flags;
#define PARSER_FLAG_COMPLETE_FRAMES 0x0001
	int64_t offset;      // byte offset from starting packet start
	int64_t last_offset;
} AVCodecParserContext;

typedef struct AVCodecParser {
	int  codec_ids[5]; /* several codec IDs are permitted */
	int  priv_data_size;
	int  (*parser_init)(AVCodecParserContext* s);
	int  (*parser_parse)(AVCodecParserContext* s, AVCodecContext *avctx, const uint8_t** poutbuf, int* poutbuf_size, const uint8_t* buf, int buf_size);
	void (*parser_close)(AVCodecParserContext* s);
	int  (*split)(AVCodecContext* avctx, const uint8_t* buf, int buf_size);
	struct AVCodecParser* next;
} AVCodecParser;

typedef struct AVPacket {
	uint8_t *data;
	int size;
} AVPacket;

/**
 * AVCodec.
 */
typedef struct AVCodec {
	/**
	 * Name of the codec implementation.
	 * The name is globally unique among encoders and among decoders (but an
	 * encoder and a decoder can share the same name).
	 * This is the primary way to find a codec from the user perspective.
	 */
	const char* name;
	enum CodecType type;
	enum CodecID id;
	int priv_data_size;
	int (*init)(AVCodecContext* avctx);
	int (*encode)(AVCodecContext* avctx, uint8_t* buf, int buf_size, void* data);
	int (*close)(AVCodecContext* avctx);
	//int (*decode)(AVCodecContext* avctx, void* outdata, int* outdata_size, uint8_t* buf, int buf_size);
	int (*decode)(AVCodecContext* avctx, void* outdata, int* outdata_size, AVPacket* avpkt);
	int capabilities;
	struct AVCodec* next;
	void (*flush)(AVCodecContext* avctx);
} AVCodec;

/*****************************************************************************/
/***   bitstream.h                                                         ***/
/*****************************************************************************/

#define NEG_SSR32(a,s) ((( int32_t)(a)) >> (32 - (s)))
#define NEG_USR32(a,s) (((uint32_t)(a)) >> (32 - (s)))

/* bit input */
/* buffer, buffer_end and size_in_bits must be present and used by every reader */
typedef struct GetBitContext {
	const uint8_t *buffer, *buffer_end;
#ifdef ALT_BITSTREAM_READER
	int index;
#elif defined LIBMPEG2_BITSTREAM_READER
	uint8_t *buffer_ptr;
	uint32_t cache;
	int bit_count;
#elif defined A32_BITSTREAM_READER
	uint32_t *buffer_ptr;
	uint32_t cache0;
	uint32_t cache1;
	int bit_count;
#endif
	int size_in_bits;
} GetBitContext;

#define VLC_TYPE int16_t

typedef struct VLC {
	int bits;
	VLC_TYPE (*table)[2]; // code, bits
	int table_size, table_allocated;
} VLC;

#define init_vlc(vlc, nb_bits, nb_codes,\
                 bits, bits_wrap, bits_size,\
                 codes, codes_wrap, codes_size,\
                 flags)\
        init_vlc_sparse(vlc, nb_bits, nb_codes,\
                 bits, bits_wrap, bits_size,\
                 codes, codes_wrap, codes_size,\
                 NULL, 0, 0, flags)

int init_vlc_sparse(VLC *vlc, int nb_bits, int nb_codes,
             const void *bits, int bits_wrap, int bits_size,
             const void *codes, int codes_wrap, int codes_size,
             const void *symbols, int symbols_wrap, int symbols_size,
             int flags);

#define INIT_VLC_LE         2
#define INIT_VLC_USE_NEW_STATIC 4
void free_vlc(VLC* vlc);

#define INIT_VLC_STATIC(vlc, bits, a,b,c,d,e,f,g, static_size)\
{\
    static VLC_TYPE table[static_size][2];\
    (vlc)->table= table;\
    (vlc)->table_allocated= static_size;\
    init_vlc(vlc, bits, a,b,c,d,e,f,g, INIT_VLC_USE_NEW_STATIC);\
}

/**
 * init GetBitContext.
 * @param buffer bitstream buffer, must be FF_INPUT_BUFFER_PADDING_SIZE bytes larger then the actual read bits
 * because some optimized bitstream readers read 32 or 64 bit at once and could read over the end
 * @param bit_size the size of the buffer in bits
 *
 * While GetBitContext stores the buffer size, for performance reasons you are
 * responsible for checking for the buffer end yourself (take advantage of the padding)!
 */
static inline void init_get_bits(GetBitContext *s,
                   const uint8_t *buffer, int bit_size)
{
    int buffer_size= (bit_size+7)>>3;
    if(buffer_size < 0 || bit_size < 0) {
        buffer_size = bit_size = 0;
        buffer = NULL;
    }

    s->buffer= buffer;
    s->size_in_bits= bit_size;
    s->buffer_end= buffer + buffer_size;
#ifdef ALT_BITSTREAM_READER
    s->index=0;
#elif defined LIBMPEG2_BITSTREAM_READER
    s->buffer_ptr = (uint8_t*)((intptr_t)buffer&(~1));
    s->bit_count = 16 + 8*((intptr_t)buffer&1);
    skip_bits_long(s, 0);
#elif defined A32_BITSTREAM_READER
    s->buffer_ptr = (uint32_t*)((intptr_t)buffer&(~3));
    s->bit_count = 32 + 8*((intptr_t)buffer&3);
    skip_bits_long(s, 0);
#endif
}

#ifdef ALT_BITSTREAM_READER

#define MIN_CACHE_BITS 25

#define OPEN_READER(name, gb)\
	int name##_index = (gb)->index;\
	int name##_cache = 0;\

#define CLOSE_READER(name, gb)\
	(gb)->index = name##_index;\

#ifdef ALT_BITSTREAM_READER_LE

#define UPDATE_CACHE(name, gb)\
	name##_cache = AV_RL32(((const uint8_t*)(gb)->buffer) + (name##_index >> 3)) >> (name##_index & 0x07);

#define SKIP_CACHE(name, gb, num)\
	name##_cache >>= (num);

#else

#define UPDATE_CACHE(name, gb)\
	name##_cache = AV_RB32(((const uint8_t *)(gb)->buffer) + (name##_index >> 3) ) << (name##_index & 0x07);

#define SKIP_CACHE(name, gb, num)\
	name##_cache <<= (num);

#endif

#define SKIP_COUNTER(name, gb, num)\
	name##_index += (num);

#define SKIP_BITS(name, gb, num)\
	{\
		SKIP_CACHE(name, gb, num)\
		SKIP_COUNTER(name, gb, num)\
	}

#define LAST_SKIP_BITS(name, gb, num) SKIP_COUNTER(name, gb, num)
#define LAST_SKIP_CACHE(name, gb, num) ;

#ifdef ALT_BITSTREAM_READER_LE

#define SHOW_UBITS(name, gb, num)\
	((name##_cache) & (NEG_USR32(0xffffffff, num)))

#define SHOW_SBITS(name, gb, num)\
	NEG_SSR32((name##_cache) << (32 - (num)), num)

#else

#define SHOW_UBITS(name, gb, num)\
	NEG_USR32(name##_cache, num)

#define SHOW_SBITS(name, gb, num)\
	NEG_SSR32(name##_cache, num)

# endif

#endif /* ALT_BITSTREAM_READER */

static inline int get_bits_count(GetBitContext* s) {
	return s->index;
}

static inline int get_sbits(GetBitContext* s, int n) {
  int tmp;
  OPEN_READER(re, s)
  UPDATE_CACHE(re, s)
  tmp = SHOW_SBITS(re, s, n);
  LAST_SKIP_BITS(re, s, n)
  CLOSE_READER(re, s)
  return tmp;
}

/**
 * reads 1-17 bits.
 * Note, the alt bitstream reader can read up to 25 bits, but the libmpeg2 reader can't
 */
static inline unsigned int get_bits(GetBitContext* s, int n) {
	int tmp;
	OPEN_READER(re, s)
	UPDATE_CACHE(re, s)
	tmp = SHOW_UBITS(re, s, n);
	LAST_SKIP_BITS(re, s, n)
	CLOSE_READER(re, s)
	return tmp;
}

static inline unsigned int get_bits1(GetBitContext* s) {
#ifdef ALT_BITSTREAM_READER
	int index = s->index;
	uint8_t result = s->buffer[index >> 3];
#ifdef ALT_BITSTREAM_READER_LE
	result >>= (index & 0x07);
	result &= 1;
#else
	result <<= (index & 0x07);
	result >>= 8 - 1;
#endif
	index++;
	s->index = index;
	return result;
#else
	return get_bits(s, 1);
#endif
}

static inline void skip_bits(GetBitContext* s, int n) {
	OPEN_READER(re, s)
	UPDATE_CACHE(re, s)
	LAST_SKIP_BITS(re, s, n)
	CLOSE_READER(re, s)
}

static inline void skip_bits1(GetBitContext* s) {
	skip_bits(s, 1);
}

static inline void skip_bits_long(GetBitContext* s, int n) {
	s->index += n;
}

/**
 * reads 0-32 bits.
 */
static inline unsigned int get_bits_long(GetBitContext* s, int n) {
	if (n <= 17)
		return get_bits(s, n);
	else {
#ifdef ALT_BITSTREAM_READER_LE
		int ret = get_bits(s, 16);
		return ret | (get_bits(s, n - 16) << 16);
#else
		int ret= get_bits(s, 16) << (n - 16);
		return ret | get_bits(s, n - 16);
#endif
	}
}

/**
 * shows 1-17 bits.
 * Note, the alt bitstream reader can read up to 25 bits, but the libmpeg2 reader can't
 */
static inline unsigned int show_bits(GetBitContext* s, int n) {
	int tmp;
	OPEN_READER(re, s)
	UPDATE_CACHE(re, s)
	tmp = SHOW_UBITS(re, s, n);
	//	CLOSE_READER(re, s)
	return tmp;
}

/**
 * shows 0-32 bits.
 */
static inline unsigned int show_bits_long(GetBitContext* s, int n) {
  if (n <= 17) return show_bits(s, n);
  else {
		GetBitContext gb = *s;
		int ret = get_bits_long(s, n);
		*s= gb;
		return ret;
  }
}

/**
 *
 * if the vlc code is invalid and max_depth=1 than no bits will be removed
 * if the vlc code is invalid and max_depth>1 than the number of bits removed
 * is undefined
 */
#define GET_VLC(code, name, gb, table, bits, max_depth)\
{\
    int n, index, nb_bits;\
\
    index= SHOW_UBITS(name, gb, bits);\
    code = table[index][0];\
    n    = table[index][1];\
\
    if(max_depth > 1 && n < 0){\
        LAST_SKIP_BITS(name, gb, bits)\
        UPDATE_CACHE(name, gb)\
\
        nb_bits = -n;\
\
        index= SHOW_UBITS(name, gb, nb_bits) + code;\
        code = table[index][0];\
        n    = table[index][1];\
        if(max_depth > 2 && n < 0){\
            LAST_SKIP_BITS(name, gb, nb_bits)\
            UPDATE_CACHE(name, gb)\
\
            nb_bits = -n;\
\
            index= SHOW_UBITS(name, gb, nb_bits) + code;\
            code = table[index][0];\
            n    = table[index][1];\
        }\
    }\
    SKIP_BITS(name, gb, n)\
}

/**
 * parses a vlc code, faster then get_vlc()
 * @param bits is the number of bits which will be read at once, must be
 *             identical to nb_bits in init_vlc()
 * @param max_depth is the number of times bits bits must be read to completely
 *                  read the longest vlc code
 *                  = (max_vlc_length + bits - 1) / bits
 */
static inline int get_vlc2(GetBitContext *s, VLC_TYPE (*table)[2],
                                  int bits, int max_depth)
{
    int code;

    OPEN_READER(re, s)
    UPDATE_CACHE(re, s)

    GET_VLC(code, re, s, table, bits, max_depth)

    CLOSE_READER(re, s)
    return code;
}

/*****************************************************************************/
/***   crc.h                                                               ***/
/*****************************************************************************/

typedef uint32_t AVCRC;

typedef enum {
	AV_CRC_8_ATM,
	AV_CRC_16_ANSI,
	AV_CRC_16_CCITT,
	AV_CRC_32_IEEE,
	AV_CRC_32_IEEE_LE,  /* reversed bitorder version of AV_CRC_32_IEEE */
	AV_CRC_MAX,         /* not part of public API! don't use outside lavu */
} AVCRCId;

int av_crc_init(AVCRC* ctx, int le, int bits, uint32_t poly, int ctx_size);
const AVCRC* av_crc_get_table(AVCRCId crc_id);
uint32_t av_crc(const AVCRC* ctx, uint32_t start_crc, const uint8_t* buffer, size_t length);

/*****************************************************************************/
/***   log.h                                                               ***/
/*****************************************************************************/

/**
 * something went really wrong and we will crash now
 */
#define AV_LOG_PANIC     0

/**
 * something went wrong and recovery is not possible
 * like no header in a format which depends on it or a combination
 * of parameters which are not allowed
 */
#define AV_LOG_FATAL     8

/**
 * something went wrong and cannot losslessly be recovered
 * but not all future data is affected
 */
#define AV_LOG_ERROR    16

/**
 * something somehow does not look correct / something which may or may not
 * lead to some problems like use of -vstrict -2
 */
#define AV_LOG_WARNING  24

#define AV_LOG_INFO     32
#define AV_LOG_VERBOSE  40

/**
 * stuff which is only useful for libav* developers
 */
#define AV_LOG_DEBUG    48

extern int mlp_av_log_level;

extern void av_log(void* avctx, int level, const char* fmt, ...);
extern void mlp_av_vlog(void* avctx, int level, const char* fmt, va_list);
extern int  mlp_av_log_get_level();
extern void mlp_av_log_set_level(int level);
extern void mlp_av_log_set_callback(void (*callback)(void* avctx, int level, const char* fmt, va_list vl));
extern void mlp_av_log_default_callback(void* avctx, int level, const char* fmt, va_list vl);

//extern void dprintf(AVCodecContext* avctx, const char* fmt, ...);

/*****************************************************************************/
/***   parser.h                                                            ***/
/*****************************************************************************/

typedef struct ParseContext {
	uint8_t* buffer;
	int index;
	int last_index;
	unsigned int buffer_size;
	uint32_t state;             // contains the last few bytes in MSB order
	int frame_start_found;
	int overread;               // the number of bytes which where irreversibly read from the next frame
	int overread_index;         // the index into ParseContext.buffer of the overread bytes
} ParseContext;

#define END_NOT_FOUND (-100)

int ff_combine_frame(ParseContext* pc, int next, const uint8_t** buf, int* buf_size);

/*****************************************************************************/
/***   dsputil.h                                                           ***/
/*****************************************************************************/

/**
 * DSPContext.
 */
typedef struct DSPContext {
    /* mlp/truehd functions */
    void (*mlp_filter_channel)(int32_t *state, const int32_t *coeff,
                               int firorder, int iirorder,
                               unsigned int filter_shift, int32_t mask, int blocksize,
                               int32_t *sample_buffer);
} DSPContext;

void dsputil_init(DSPContext* p, AVCodecContext *avctx);

#define CONFIG_MLP_DECODER 1
#define ARCH_X86 1

#endif /* MLP_UTIL_H */
