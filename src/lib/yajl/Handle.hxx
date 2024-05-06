// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <yajl_parse.h>

#include <utility>

namespace Yajl {

/**
 * OO wrapper for a #yajl_handle.
 */
class Handle {
	yajl_handle handle = nullptr;

public:
	Handle() = default;

	Handle(const yajl_callbacks *callbacks,
	       yajl_alloc_funcs *afs,
	       void *ctx) noexcept
		:handle(yajl_alloc(callbacks, afs, ctx)) {}

	Handle(Handle &&src) noexcept
		:handle(std::exchange(src.handle, nullptr)) {}

	~Handle() noexcept {
		if (handle != nullptr)
			yajl_free(handle);
	}

	Handle &operator=(Handle &&src) noexcept {
		std::swap(handle, src.handle);
		return *this;
	}

	void Parse(const unsigned char *jsonText, size_t jsonTextLength) {
		HandleStatus(yajl_parse(handle, jsonText, jsonTextLength));
	}

	void CompleteParse() {
		HandleStatus(yajl_complete_parse(handle));
	}

private:
	void HandleStatus(yajl_status status) {
		if (status == yajl_status_error)
			ThrowError();
	}

	[[noreturn]]
	void ThrowError();
};

} // namespace Yajl
