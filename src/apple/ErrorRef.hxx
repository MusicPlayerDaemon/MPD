// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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
