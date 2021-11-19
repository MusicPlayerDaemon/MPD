/*
 * Copyright 2018-2021 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef YAJL_HANDLE_HXX
#define YAJL_HANDLE_HXX

#include <yajl/yajl_parse.h>

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

#endif
