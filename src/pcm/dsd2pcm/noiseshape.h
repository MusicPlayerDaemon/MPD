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

#ifndef NOISE_SHAPE_H_INCLUDED
#define NOISE_SHAPE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct noise_shape_ctx_s {
	int sos_count;      /* number of second order sections */
	const float *bbaa;  /* filter coefficients, owned by user */
	float *t1, *t2;     /* filter state, owned by ns library */
} noise_shape_ctx;

/**
 * initializes a noise_shaper context
 * returns an error code or 0
 */
extern int noise_shape_init(
	noise_shape_ctx *ctx,
	int sos_count,
	const float *coeffs);

/**
 * destroys a noise_shaper context
 */
extern void noise_shape_destroy(
	noise_shape_ctx *ctx);

/**
 * initializes a noise_shaper context so that its state
 * is a copy of a given context
 * returns an error code or 0
 */
extern int noise_shape_clone(
	const noise_shape_ctx *from, noise_shape_ctx *to);

/**
 * computes the next "noise shaping sample". Note: This call
 * alters the internal state. xxx_get and xxx_update must be
 * called in an alternating manner.
 */
extern float noise_shape_get(
	noise_shape_ctx *ctx);

/**
 * updates the noise shaper's state with the
 * last quantization error
 */
extern void noise_shape_update(
	noise_shape_ctx *ctx, float qerror);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NOISE_SHAPE_H_INCLUDED */

