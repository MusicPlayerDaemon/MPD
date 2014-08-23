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

#ifndef NOISE_SHAPE_HXX_INCLUDED
#define NOISE_SHAPE_HXX_INCLUDED

#include <stdexcept>
#include "noiseshape.h"

/**
 * C++ wrapper for the noiseshape C library
 */

class noise_shaper
{
	noise_shape_ctx ctx;
public:
	noise_shaper(int sos_count, const float *bbaa)
	{
		noise_shape_init(&ctx, sos_count, bbaa);
	}

	noise_shaper(noise_shaper const& x)
	{
		noise_shape_clone(&x.ctx,&ctx);
	}

	~noise_shaper()
	{ noise_shape_destroy(&ctx); }

	noise_shaper& operator=(noise_shaper const& x)
	{
		if (this != &x) {
			noise_shape_destroy(&ctx);
			noise_shape_clone(&x.ctx,&ctx);
		}
		return *this;
	}

	float get() { return noise_shape_get(&ctx); }

	void update(float error) { noise_shape_update(&ctx,error); }
};

#endif /* NOISE_SHAPE_HXX_INCLUDED */

