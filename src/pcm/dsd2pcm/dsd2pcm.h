#ifndef DSD2PCM_H_INCLUDED
#define DSD2PCM_H_INCLUDED

#include <stddef.h>
#include <string.h>

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

