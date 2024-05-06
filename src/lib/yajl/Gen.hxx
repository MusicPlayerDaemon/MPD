// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <yajl_gen.h>

#include <algorithm>
#include <span>
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

	std::span<const char> GetBuffer() const noexcept {
		const unsigned char *buf;
		size_t len;
		auto status = yajl_gen_get_buf(gen, &buf, &len);
		if (status != yajl_gen_status_ok)
			return {};

		return {(const char *)buf, len};
	}
};

} // namespace Yajl
