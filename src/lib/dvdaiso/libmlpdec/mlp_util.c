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

#include "mlp_util.h"

void av_freep(void* arg);
void* ff_realloc_static(void* ptr, unsigned int size);

/*****************************************************************************/
/***   utils.c                                                             ***/
/*****************************************************************************/

void *av_fast_realloc(void *ptr, unsigned int *size, unsigned int min_size)
{
    if(min_size < *size)
        return ptr;

    *size= FFMAX(17*min_size/16 + 32, min_size);

    ptr= av_realloc(ptr, *size);
    if(!ptr) //we could set this to the unmodified min_size but this is safer if the user lost the ptr and uses NULL now
        *size= 0;

    return ptr;
}

/*****************************************************************************/
/***   bitstream.c                                                         ***/
/*****************************************************************************/

#define GET_DATA(v, table, i, wrap, size) \
{\
    const uint8_t *ptr = (const uint8_t *)table + i * wrap;\
    switch(size) {\
    case 1:\
        v = *(const uint8_t *)ptr;\
        break;\
    case 2:\
        v = *(const uint16_t *)ptr;\
        break;\
    default:\
        v = *(const uint32_t *)ptr;\
        break;\
    }\
}

static int alloc_table(VLC *vlc, int size, int use_static)
{
    int index;
    index = vlc->table_size;
    vlc->table_size += size;
    if (vlc->table_size > vlc->table_allocated) {
        if(use_static)
            abort(); //cant do anything, init_vlc() is used with too little memory
        vlc->table_allocated += (1 << vlc->bits);
        vlc->table = av_realloc(vlc->table,
                                sizeof(VLC_TYPE) * 2 * vlc->table_allocated);
        if (!vlc->table)
            return -1;
    }
    return index;
}

static int build_table(VLC *vlc, int table_nb_bits,
                       int nb_codes,
                       const void *bits, int bits_wrap, int bits_size,
                       const void *codes, int codes_wrap, int codes_size,
                       const void *symbols, int symbols_wrap, int symbols_size,
                       uint32_t code_prefix, int n_prefix, int flags)
{
    int i, j, k, n, table_size, table_index, nb, n1, index, code_prefix2, symbol;
    uint32_t code;
    VLC_TYPE (*table)[2];

    table_size = 1 << table_nb_bits;
    table_index = alloc_table(vlc, table_size, flags & INIT_VLC_USE_NEW_STATIC);
#ifdef DEBUG_VLC
    av_log(NULL,AV_LOG_DEBUG,"new table index=%d size=%d code_prefix=%x n=%d\n",
           table_index, table_size, code_prefix, n_prefix);
#endif
    if (table_index < 0)
        return -1;
    table = &vlc->table[table_index];

    for(i=0;i<table_size;i++) {
        table[i][1] = 0; //bits
        table[i][0] = -1; //codes
    }

    /* first pass: map codes and compute auxillary table sizes */
    for(i=0;i<nb_codes;i++) {
        GET_DATA(n, bits, i, bits_wrap, bits_size);
        GET_DATA(code, codes, i, codes_wrap, codes_size);
        /* we accept tables with holes */
        if (n <= 0)
            continue;
        if (!symbols)
            symbol = i;
        else
            GET_DATA(symbol, symbols, i, symbols_wrap, symbols_size);
#if defined(DEBUG_VLC) && 0
        av_log(NULL,AV_LOG_DEBUG,"i=%d n=%d code=0x%x\n", i, n, code);
#endif
        /* if code matches the prefix, it is in the table */
        n -= n_prefix;
        if(flags & INIT_VLC_LE)
            code_prefix2= code & (n_prefix>=32 ? 0xffffffff : (1 << n_prefix)-1);
        else
            code_prefix2= code >> n;
        if (n > 0 && code_prefix2 == code_prefix) {
            if (n <= table_nb_bits) {
                /* no need to add another table */
                j = (code << (table_nb_bits - n)) & (table_size - 1);
                nb = 1 << (table_nb_bits - n);
                for(k=0;k<nb;k++) {
                    if(flags & INIT_VLC_LE)
                        j = (code >> n_prefix) + (k<<n);
#ifdef DEBUG_VLC
                    av_log(NULL, AV_LOG_DEBUG, "%4x: code=%d n=%d\n",
                           j, i, n);
#endif
                    if (table[j][1] /*bits*/ != 0) {
                        av_log(NULL, AV_LOG_ERROR, "incorrect codes\n");
                        return -1;
                    }
                    table[j][1] = n; //bits
                    table[j][0] = symbol;
                    j++;
                }
            } else {
                n -= table_nb_bits;
                j = (code >> ((flags & INIT_VLC_LE) ? n_prefix : n)) & ((1 << table_nb_bits) - 1);
#ifdef DEBUG_VLC
                av_log(NULL,AV_LOG_DEBUG,"%4x: n=%d (subtable)\n",
                       j, n);
#endif
                /* compute table size */
                n1 = -table[j][1]; //bits
                if (n > n1)
                    n1 = n;
                table[j][1] = -n1; //bits
            }
        }
    }

    /* second pass : fill auxillary tables recursively */
    for(i=0;i<table_size;i++) {
        n = table[i][1]; //bits
        if (n < 0) {
            n = -n;
            if (n > table_nb_bits) {
                n = table_nb_bits;
                table[i][1] = -n; //bits
            }
            index = build_table(vlc, n, nb_codes,
                                bits, bits_wrap, bits_size,
                                codes, codes_wrap, codes_size,
                                symbols, symbols_wrap, symbols_size,
                                (flags & INIT_VLC_LE) ? (code_prefix | (i << n_prefix)) : ((code_prefix << table_nb_bits) | i),
                                n_prefix + table_nb_bits, flags);
            if (index < 0)
                return -1;
            /* note: realloc has been done, so reload tables */
            table = &vlc->table[table_index];
            table[i][0] = index; //code
        }
    }
    return table_index;
}

/* Build VLC decoding tables suitable for use with get_vlc().

   'nb_bits' set thee decoding table size (2^nb_bits) entries. The
   bigger it is, the faster is the decoding. But it should not be too
   big to save memory and L1 cache. '9' is a good compromise.

   'nb_codes' : number of vlcs codes

   'bits' : table which gives the size (in bits) of each vlc code.

   'codes' : table which gives the bit pattern of of each vlc code.

   'symbols' : table which gives the values to be returned from get_vlc().

   'xxx_wrap' : give the number of bytes between each entry of the
   'bits' or 'codes' tables.

   'xxx_size' : gives the number of bytes of each entry of the 'bits'
   or 'codes' tables.

   'wrap' and 'size' allows to use any memory configuration and types
   (byte/word/long) to store the 'bits', 'codes', and 'symbols' tables.

   'use_static' should be set to 1 for tables, which should be freed
   with av_free_static(), 0 if free_vlc() will be used.
*/
int init_vlc_sparse(VLC *vlc, int nb_bits, int nb_codes,
             const void *bits, int bits_wrap, int bits_size,
             const void *codes, int codes_wrap, int codes_size,
             const void *symbols, int symbols_wrap, int symbols_size,
             int flags)
{
    vlc->bits = nb_bits;
    if(flags & INIT_VLC_USE_NEW_STATIC){
        if(vlc->table_size && vlc->table_size == vlc->table_allocated){
            return 0;
        }else if(vlc->table_size){
            abort(); // fatal error, we are called on a partially initialized table
        }
    }else {
        vlc->table = NULL;
        vlc->table_allocated = 0;
        vlc->table_size = 0;
    }

#ifdef DEBUG_VLC
    av_log(NULL,AV_LOG_DEBUG,"build table nb_codes=%d\n", nb_codes);
#endif

    if (build_table(vlc, nb_bits, nb_codes,
                    bits, bits_wrap, bits_size,
                    codes, codes_wrap, codes_size,
                    symbols, symbols_wrap, symbols_size,
                    0, 0, flags) < 0) {
        av_freep(&vlc->table);
        return -1;
    }
    if((flags & INIT_VLC_USE_NEW_STATIC) && vlc->table_size != vlc->table_allocated)
        av_log(NULL, AV_LOG_ERROR, "needed %d had %d\n", vlc->table_size, vlc->table_allocated);
    return 0;
}

/*****************************************************************************/
/***   crc.c                                                               ***/
/*****************************************************************************/

/**
 * Initializes a CRC table.
 * @param ctx must be an array of size sizeof(AVCRC)*257 or sizeof(AVCRC)*1024
 * @param cts_size size of ctx in bytes
 * @param le If 1, the lowest bit represents the coefficient for the highest
 *           exponent of the corresponding polynomial (both for poly and
 *           actual CRC).
 *           If 0, you must swap the CRC parameter and the result of av_crc
 *           if you need the standard representation (can be simplified in
 *           most cases to e.g. bswap16):
 *           bswap_32(crc << (32-bits))
 * @param bits number of bits for the CRC
 * @param poly generator polynomial without the x**bits coefficient, in the
 *             representation as specified by le
 * @return <0 on failure
 */
int av_crc_init(AVCRC *ctx, int le, int bits, uint32_t poly, int ctx_size){
    int i, j;
    uint32_t c;

    if (bits < 8 || bits > 32 || poly >= (1LL<<bits))
        return -1;
    if (ctx_size != sizeof(AVCRC)*257 && ctx_size != sizeof(AVCRC)*1024)
        return -1;

    for (i = 0; i < 256; i++) {
        if (le) {
            for (c = i, j = 0; j < 8; j++)
                c = (c>>1)^(poly & (-(c&1)));
            ctx[i] = c;
        } else {
            for (c = i << 24, j = 0; j < 8; j++)
                c = (c<<1) ^ ((poly<<(32-bits)) & (((int32_t)c)>>31) );
            ctx[i] = bswap_32(c);
        }
    }
    ctx[256]=1;
#if !CONFIG_SMALL
    if(ctx_size >= sizeof(AVCRC)*1024)
        for (i = 0; i < 256; i++)
            for(j=0; j<3; j++)
                ctx[256*(j+1) + i]= (ctx[256*j + i]>>8) ^ ctx[ ctx[256*j + i]&0xFF ];
#endif

    return 0;
}

/**
 * Calculates the CRC of a block.
 * @param crc CRC of previous blocks if any or initial value for CRC
 * @return CRC updated with the data from the given block
 *
 * @see av_crc_init() "le" parameter
 */
uint32_t av_crc(const AVCRC *ctx, uint32_t crc, const uint8_t *buffer, size_t length){
    const uint8_t *end= buffer+length;

#if !CONFIG_SMALL
    if(!ctx[256]) {
        while(((intptr_t) buffer & 3) && buffer < end)
            crc = ctx[((uint8_t)crc) ^ *buffer++] ^ (crc >> 8);

        while(buffer<end-3){
            crc ^= le2me_32(*(const uint32_t*)buffer); buffer+=4;
            crc =  ctx[3*256 + ( crc     &0xFF)]
                  ^ctx[2*256 + ((crc>>8 )&0xFF)]
                  ^ctx[1*256 + ((crc>>16)&0xFF)]
                  ^ctx[0*256 + ((crc>>24)     )];
        }
    }
#endif
    while(buffer<end)
        crc = ctx[((uint8_t)crc) ^ *buffer++] ^ (crc >> 8);

    return crc;
}

/*****************************************************************************/
/***   log.h                                                               ***/
/*****************************************************************************/

int mlp_av_log_level = AV_LOG_INFO;

void mlp_av_log_default_callback(void* avctx, int level, const char* fmt, va_list vl) {
	if (level > mlp_av_log_level)
		return;
	vfprintf(stderr, fmt, vl);
}

static void (*mlp_av_log_callback)(void* avctx, int level , const char* fmt, va_list vl) = mlp_av_log_default_callback;

void av_log(void* avctx, int level, const char* fmt, ...) {
	va_list vl;
	va_start(vl, fmt);
	mlp_av_vlog(avctx, level, fmt, vl);
	va_end(vl);
}

void mlp_av_vlog(void* avcl, int level, const char *fmt, va_list vl) {
	mlp_av_log_callback(avcl, level, fmt, vl);
}

int mlp_av_log_get_level() {
	return mlp_av_log_level;
}

void mlp_av_log_set_level(int level) {
	mlp_av_log_level = level;
}

void mlp_av_log_set_callback(void (*callback)(void* avctx, int level , const char* fmt, va_list vl)) {
	mlp_av_log_callback = callback;
}

/*
void dprintf(AVCodecContext* avctx, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
}
*/

/*****************************************************************************/
/***   mem.h                                                               ***/
/*****************************************************************************/

inline void* av_malloc(unsigned int size) {
	return malloc(size);
}

void* av_realloc(void* ptr, unsigned int size) {
	return realloc(ptr, size);
}

void av_free(void* ptr) {
	if (ptr)
		free(ptr);
}

void av_freep(void* arg) {
	void** ptr= (void**)arg;
	av_free(*ptr);
	*ptr = NULL;
}

void* av_mallocz(unsigned int size) {
	void* ptr;
	ptr = av_malloc(size);
	if (ptr)
		memset(ptr, 0, size);
	return ptr;
}

/*****************************************************************************/
/***   parser.c                                                            ***/
/*****************************************************************************/

/**
 * combines the (truncated) bitstream to a complete frame
 * @returns -1 if no complete frame could be created
 */
int ff_combine_frame(ParseContext* pc, int next, const uint8_t** buf, int* buf_size) {

	/* Copy overread bytes from last frame into buffer. */
  for (; pc->overread > 0; pc->overread--) {
		pc->buffer[pc->index++] = pc->buffer[pc->overread_index++];
  }

  /* flush remaining if EOF */
  if (!*buf_size && next == END_NOT_FOUND) {
		next= 0;
  }

  pc->last_index = pc->index;

  /* copy into buffer end return */
  if (next == END_NOT_FOUND) {
		pc->buffer = (uint8_t*)av_fast_realloc(pc->buffer, &pc->buffer_size, (*buf_size) + pc->index + FF_INPUT_BUFFER_PADDING_SIZE);

		memcpy(&pc->buffer[pc->index], *buf, *buf_size);
		pc->index += *buf_size;
		return -1;
  }

  *buf_size = pc->overread_index = pc->index + next;

  /* append to buffer */
  if (pc->index) {
		pc->buffer = (uint8_t*)av_fast_realloc(pc->buffer, &pc->buffer_size, next + pc->index + FF_INPUT_BUFFER_PADDING_SIZE);

		memcpy(&pc->buffer[pc->index], *buf, next + FF_INPUT_BUFFER_PADDING_SIZE);
		pc->index = 0;
		*buf = pc->buffer;
  }

  /* store overread bytes */
  for(; next < 0; next++){
		pc->state = (pc->state<<8) | pc->buffer[pc->last_index + next];
		pc->overread++;
  }
  return 0;
}

/*****************************************************************************/
/***   utils.c                                                             ***/
/*****************************************************************************/

static unsigned int last_static = 0;
static unsigned int allocated_static = 0;
static void** array_static = NULL;

void* av_mallocz_static(unsigned int size) {
	void* ptr = av_mallocz(size);
	if (ptr) {
		array_static = (void**)av_fast_realloc(array_static, &allocated_static, sizeof(void*)*(last_static + 1));
		if (!array_static)
			return NULL;
		array_static[last_static++] = ptr;
	}
	return ptr;
}

void* ff_realloc_static(void* ptr, unsigned int size) {
	int i;
	if (!ptr)
		return av_mallocz_static(size);
	/* Look for the old ptr */
	for (i = 0; i < last_static; i++) {
		if (array_static[i] == ptr) {
			array_static[i] = av_realloc(array_static[i], size);
			return array_static[i];
		}
	}
	return NULL;
}

/*****************************************************************************/
/***   mlpdsp.c                                                            ***/
/*****************************************************************************/

void dsputil_init(DSPContext* p, AVCodecContext *avctx) {
	 ff_mlp_init(p, avctx);
}

void ff_mlp_init_x86(DSPContext* c, AVCodecContext *avctx) {
}
