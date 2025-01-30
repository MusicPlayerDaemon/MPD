// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef APPLE_STRING_REF_HXX
#define APPLE_STRING_REF_HXX

#include <CoreFoundation/CFString.h>

#include <utility>

namespace Apple {

class StringRef {
	CFStringRef ref = nullptr;

public:
	explicit StringRef(CFStringRef _ref) noexcept
		:ref(_ref) {}

	StringRef(StringRef &&src) noexcept
		:ref(std::exchange(src.ref, nullptr)) {}

	~StringRef() noexcept {
		if (ref)
			CFRelease(ref);
	}

	StringRef &operator=(StringRef &&src) noexcept {
		using std::swap;
		swap(ref, src.ref);
		return *this;
	}

	operator bool() const noexcept {
		return ref != nullptr;
	}

	bool GetCString(char *buffer, std::size_t size,
			CFStringEncoding encoding=kCFStringEncodingUTF8) const noexcept
	{
		return CFStringGetCString(ref, buffer, size, encoding);
	}
};

} // namespace Apple

#endif
