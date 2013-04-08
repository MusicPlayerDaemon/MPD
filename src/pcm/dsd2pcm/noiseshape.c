#include <stdlib.h>
#include <string.h>

#include "noiseshape.h"

extern int noise_shape_init(
	noise_shape_ctx *ctx,
	int sos_count,
	const float *coeffs)
{
	int i;
	ctx->sos_count = sos_count;
	ctx->bbaa = coeffs;
	ctx->t1 = (float*) malloc(sizeof(float)*sos_count);
	if (!ctx->t1) goto escape1;
	ctx->t2 = (float*) malloc(sizeof(float)*sos_count);
	if (!ctx->t2) goto escape2;
	for (i=0; i<sos_count; ++i) {
		ctx->t1[i] = 0.f;
		ctx->t2[i] = 0.f;
	}
	return 0;
escape2:
	free(ctx->t1);
escape1:
	return -1;
}

extern void noise_shape_destroy(
	noise_shape_ctx *ctx)
{
	free(ctx->t1);
	free(ctx->t2);
}

extern int noise_shape_clone(
	const noise_shape_ctx *from,
	noise_shape_ctx *to)
{
	to->sos_count = from->sos_count;
	to->bbaa = from->bbaa;
	to->t1 = (float*) malloc(sizeof(float)*to->sos_count);
	if (!to->t1) goto error1;
	to->t2 = (float*) malloc(sizeof(float)*to->sos_count);
	if (!to->t2) goto error2;
	memcpy(to->t1,from->t1,sizeof(float)*to->sos_count);
	memcpy(to->t2,from->t2,sizeof(float)*to->sos_count);
	return 0;
error2:
	free(to->t1);
error1:
	return -1;
}

extern float noise_shape_get(noise_shape_ctx *ctx)
{
	int i;
	float acc;
	const float *c;
	acc = 0.0;
	c = ctx->bbaa;
	for (i=0; i<ctx->sos_count; ++i) {
		float t1i = ctx->t1[i];
		float t2i = ctx->t2[i];
		ctx->t2[i] = acc -= t1i * c[2] + t2i * c[3];
		acc += t1i * c[0] + t2i * c[1];
		c += 4;
	}
	return acc;
}

extern void noise_shape_update(noise_shape_ctx *ctx, float qerror)
{
	float *p;
	int i;
	for (i=0; i<ctx->sos_count; ++i) {
		ctx->t2[i] += qerror;
	}
	p = ctx->t1;
	ctx->t1 = ctx->t2;
	ctx->t2 = p;
}

