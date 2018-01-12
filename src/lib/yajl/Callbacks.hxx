/*
 * Copyright (C) 2018 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef YAJL_CALLBACKS_HXX
#define YAJL_CALLBACKS_HXX

#include "util/Cast.hxx"
#include "util/StringView.hxx"

#include <yajl/yajl_parse.h>

namespace Yajl {

/**
 * Helper template which allows implementing callbacks as regular
 * methods.  The "ctx" parameter is casted to the enclosing class.
 */
template<class T>
struct CallbacksWrapper {
	static T &Cast(void *ctx) {
		return *(T *)ctx;
	}

	static int Integer(void *ctx, long long integerVal) noexcept {
		return Cast(ctx).Integer(integerVal);
	}

	static int String(void *ctx, const unsigned char *stringVal,
			  size_t stringLen) noexcept {
		return Cast(ctx).String(StringView((const char *)stringVal,
						   stringLen));
	}

	static int StartMap(void *ctx) noexcept {
		return Cast(ctx).StartMap();
	}

	static int MapKey(void *ctx, const unsigned char *key,
			  size_t stringLen) noexcept {
		return Cast(ctx).MapKey(StringView((const char *)key,
						   stringLen));
	}

	static int EndMap(void *ctx) noexcept {
		return Cast(ctx).EndMap();
	}

	static int StartArray(void *ctx) noexcept {
		return Cast(ctx).StartArray();
	}

	static int EndArray(void *ctx) noexcept {
		return Cast(ctx).EndArray();
	}
};

} // namespace Yajl

#endif
