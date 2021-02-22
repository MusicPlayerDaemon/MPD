/*
 * Copyright 2021 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef YAJL_GEN_HXX
#define YAJL_GEN_HXX

#include "util/ConstBuffer.hxx"

#include <yajl/yajl_gen.h>

#include <algorithm>
#include <string_view>

namespace Yajl {

/**
 * OO wrapper for #yajl_gen.
 */
class Gen {
	yajl_gen gen = nullptr;

public:
	Gen() = default;

	explicit Gen(const yajl_alloc_funcs *allocFuncs) noexcept
		:gen(yajl_gen_alloc(allocFuncs)) {}

	Gen(Gen &&src) noexcept
		:gen(std::exchange(src.gen, nullptr)) {}

	~Gen() noexcept {
		if (gen != nullptr)
			yajl_gen_free(gen);
	}

	Gen &operator=(Gen &&src) noexcept {
		using std::swap;
		swap(gen, src.gen);
		return *this;
	}

	void Integer(long long int number) noexcept {
		yajl_gen_integer(gen, number);
	}

	void String(std::string_view s) noexcept {
		yajl_gen_string(gen, (const unsigned char *)s.data(), s.size());
	}

	void OpenMap() noexcept {
		yajl_gen_map_open(gen);
	}

	void CloseMap() noexcept {
		yajl_gen_map_close(gen);
	}

	void OpenArray() noexcept {
		yajl_gen_array_open(gen);
	}

	void CloseArray() noexcept {
		yajl_gen_array_close(gen);
	}

	ConstBuffer<char> GetBuffer() const noexcept {
		const unsigned char *buf;
		size_t len;
		auto status = yajl_gen_get_buf(gen, &buf, &len);
		if (status != yajl_gen_status_ok)
			return nullptr;

		return {(const char *)buf, len};
	}
};

} // namespace Yajl

#endif
