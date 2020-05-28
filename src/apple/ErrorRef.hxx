/*
 * Copyright 2020 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef APPLE_ERROR_REF_HXX
#define APPLE_ERROR_REF_HXX

#include <CoreFoundation/CFError.h>

#include <utility>

namespace Apple {

class ErrorRef {
	CFErrorRef ref = nullptr;

public:
	explicit ErrorRef(CFErrorRef _ref) noexcept
		:ref(_ref) {}

	ErrorRef(CFAllocatorRef allocator, CFErrorDomain domain,
		 CFIndex code, CFDictionaryRef userInfo) noexcept
		:ref(CFErrorCreate(allocator, domain, code, userInfo)) {}

	ErrorRef(ErrorRef &&src) noexcept
		:ref(std::exchange(src.ref, nullptr)) {}

	~ErrorRef() noexcept {
		if (ref)
			CFRelease(ref);
	}

	ErrorRef &operator=(ErrorRef &&src) noexcept {
		using std::swap;
		swap(ref, src.ref);
		return *this;
	}

	operator bool() const noexcept {
		return ref != nullptr;
	}

	CFStringRef CopyDescription() const noexcept {
		return CFErrorCopyDescription(ref);
	}
};

} // namespace Apple

#endif
