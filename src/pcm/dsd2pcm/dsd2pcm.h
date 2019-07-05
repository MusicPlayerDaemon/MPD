/*

Copyright 2009, 2011 Sebastian Gesemann. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED BY SEBASTIAN GESEMANN ''AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEBASTIAN GESEMANN OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the
authors and should not be interpreted as representing official policies, either expressed
or implied, of Sebastian Gesemann.

 */

#ifndef DSD2PCM_H_INCLUDED
#define DSD2PCM_H_INCLUDED

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dsd2pcm_ctx_s;

typedef struct dsd2pcm_ctx_s dsd2pcm_ctx;

/**
 * initializes a "dsd2pcm engine" for one channel
 * (precomputes tables and allocates memory)
 *
 * This is the only function that is not thread-safe in terms of the
 * POSIX thread-safety definition because it modifies global state
 * (lookup tables are computed during the first call)
 */
extern dsd2pcm_ctx* dsd2pcm_init(void);

/**
 * deinitializes a "dsd2pcm engine"
 * (releases memory, don't forget!)
 */
extern void dsd2pcm_destroy(dsd2pcm_ctx *ctx);

/**
 * clones the context and returns a pointer to the
 * newly allocated copy
 */
extern dsd2pcm_ctx* dsd2pcm_clone(dsd2pcm_ctx *ctx);

/**
 * resets the internal state for a fresh new stream
 */
extern void dsd2pcm_reset(dsd2pcm_ctx *ctx);

/**
 * "translates" a stream of octets to a stream of floats
 * (8:1 decimation)
 * @param ctx -- pointer to abstract context (buffers)
 * @param samples -- number of octets/samples to "translate"
 * @param src -- pointer to first octet (input)
 * @param src_stride -- src pointer increment
 * @param lsbitfirst -- bitorder, 0=msb first, 1=lsbfirst
 * @param dst -- pointer to first float (output)
 * @param dst_stride -- dst pointer increment
 */
extern void dsd2pcm_translate(dsd2pcm_ctx *ctx,
	size_t samples,
	const unsigned char *src, ptrdiff_t src_stride,
	int lsbitfirst,
	float *dst, ptrdiff_t dst_stride);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* include guard DSD2PCM_H_INCLUDED */

