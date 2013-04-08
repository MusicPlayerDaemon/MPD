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

