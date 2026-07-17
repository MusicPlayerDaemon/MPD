// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Throw.hxx"
#include "ErrorRef.hxx"
#include "StringRef.hxx"

#include <stdexcept>
#include <string>

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
ThrowOSStatus(OSStatus status, const char *prefix)
{
	const Apple::ErrorRef cferr(nullptr, kCFErrorDomainOSStatus,
				    status, nullptr);
	const Apple::StringRef cfstr(cferr.CopyDescription());

	char description[1024];
	if (cfstr.GetCString(description, sizeof(description)))
		throw std::runtime_error(std::string{prefix} + ": " + description);

	throw std::runtime_error(prefix);
}

} // namespace Apple
