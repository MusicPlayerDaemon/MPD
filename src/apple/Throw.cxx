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
