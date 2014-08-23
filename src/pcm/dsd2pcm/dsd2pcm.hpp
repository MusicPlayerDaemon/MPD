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

