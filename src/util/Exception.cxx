// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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
