// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/Cast.hxx"

#include <string_view>

#include <yajl_parse.h>

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
		return Cast(ctx).String(std::string_view{
				(const char *)stringVal, stringLen,
			});
	}

	static int StartMap(void *ctx) noexcept {
		return Cast(ctx).StartMap();
	}

	static int MapKey(void *ctx, const unsigned char *key,
			  size_t stringLen) noexcept {
		return Cast(ctx).MapKey(std::string_view{
				(const char *)key,
				stringLen,
			});
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
