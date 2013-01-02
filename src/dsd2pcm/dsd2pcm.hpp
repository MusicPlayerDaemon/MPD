#ifndef DSD2PCM_HXX_INCLUDED
#define DSD2PCM_HXX_INCLUDED

#include <algorithm>
#include <stdexcept>
#include "dsd2pcm.h"

/**
 * C++ PImpl Wrapper for the dsd2pcm C library
 */

class dxd
{
	dsd2pcm_ctx *handle;
public:
	dxd() : handle(dsd2pcm_init()) {}

	dxd(dxd const& x) : handle(dsd2pcm_clone(x.handle)) {}

	~dxd() { dsd2pcm_destroy(handle); }

	friend void swap(dxd & a, dxd & b)
	{ std::swap(a.handle,b.handle); }

	dxd& operator=(dxd x)
	{ swap(*this,x); return *this; }

	void translate(size_t samples,
		const unsigned char *src, ptrdiff_t src_stride,
		bool lsbitfirst,
		float *dst, ptrdiff_t dst_stride)
	{
		dsd2pcm_translate(handle,samples,src,src_stride,
			lsbitfirst,dst,dst_stride);
	}
};

#endif // DSD2PCM_HXX_INCLUDED

