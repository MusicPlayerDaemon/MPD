// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Throw.hxx"
#include "ErrorRef.hxx"
#include "StringRef.hxx"

#include <cstring>
#include <stdexcept>

namespace Apple {

void
ThrowOSStatus(OSStatus status)
{
	const Apple::ErrorRef cferr(nullptr, kCFErrorDomainOSStatus,
				    status, nullptr);
	const Apple::StringRef cfstr(cferr.CopyDescription());

	char msg[1024];
	if (!cfstr.GetCString(msg, sizeof(msg)))
		throw std::runtime_error("Unknown OSStatus");

	throw std::runtime_error(msg);
}

void
ThrowOSStatus(OSStatus status, const char *_msg)
{
	const Apple::ErrorRef cferr(nullptr, kCFErrorDomainOSStatus,
				    status, nullptr);
	const Apple::StringRef cfstr(cferr.CopyDescription());

	char msg[1024];
	std::strcpy(msg, _msg);
	size_t length = std::strlen(msg);

	cfstr.GetCString(msg + length, sizeof(msg) - length);
	throw std::runtime_error(msg);
}

} // namespace Apple
