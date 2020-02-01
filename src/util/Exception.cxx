/*
 * Copyright 2016-2018 Max Kellermann <max.kellermann@gmail.com>
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

#include "Exception.hxx"

#include <utility>

template<typename T>
static void
AppendNestedMessage(std::string &result, T &&e,
		    const char *fallback, const char *separator) noexcept
{
	try {
		std::rethrow_if_nested(std::forward<T>(e));
	} catch (const std::exception &nested) {
		result += separator;
		result += nested.what();
		AppendNestedMessage(result, nested, fallback, separator);
	} catch (const std::nested_exception &ne) {
		AppendNestedMessage(result, ne, fallback, separator);
	} catch (const char *s) {
		result += separator;
		result += s;
	} catch (...) {
		result += separator;
		result += fallback;
	}
}

std::string
GetFullMessage(const std::exception &e,
	       const char *fallback, const char *separator) noexcept
{
	std::string result = e.what();
	AppendNestedMessage(result, e, fallback, separator);
	return result;
}

std::string
GetFullMessage(std::exception_ptr ep,
	       const char *fallback, const char *separator) noexcept
{
	try {
		std::rethrow_exception(std::move(ep));
	} catch (const std::exception &e) {
		return GetFullMessage(e, fallback, separator);
	} catch (const std::nested_exception &ne) {
		return GetFullMessage(ne.nested_ptr(), fallback, separator);
	} catch (const char *s) {
		return s;
	} catch (...) {
		return fallback;
	}
}
