// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Util.hxx"
#include "util/ASCII.hxx"
#include "util/SplitString.hxx"
#include "util/StringCompare.hxx"
#include "util/StringListVerify.hxx"

#include <string_view>

using std::string_view_literals::operator""sv;

bool
UriIsCanonicalRelativePath(std::string_view uri) noexcept
{
#ifdef _WIN32
	if (uri.find('\\') != uri.npos)
		/* backslash is a path separator on Windows, and would
		   allow sneaking ".." segments past the check below */
		return false;
#endif

	return IsNonEmptyListOf(uri, '/', [](std::string_view segment){
		return !segment.empty() && segment != "."sv && segment != ".."sv;
	});
}

bool
UriIsRelativePathWithinDepth(std::string_view uri, unsigned max_up) noexcept
{
	unsigned up = 0;
	while (true) {
		if (SkipPrefix(uri, "../"sv)) {
			if (++up > max_up)
				return false;
		} else if (!SkipPrefix(uri, "./"sv))
			break;
	}

	return UriIsCanonicalRelativePath(uri);
}

bool
UriStaysInsideDepth(std::string_view path, unsigned depth) noexcept
{
	for (const std::string_view segment : IterableSplitString(path, '/')) {
		if (segment.empty() || segment == "."sv)
			continue;

		if (segment == ".."sv) {
			if (depth == 0)
				return false;

			--depth;
		} else
			++depth;
	}

	return true;
}

std::string
UriRemoveAuth(const std::string_view uri) noexcept
{
	const auto scheme_end = uri.find("://"sv);
	if (scheme_end == uri.npos)
		/* not an URI with an authority component */
		return {};

	const auto authority_begin = scheme_end + 3;
	const auto authority_end = uri.find_first_of("/?#"sv, authority_begin);
	const auto auth_end = uri.rfind('@', authority_end);
	if (auth_end == uri.npos || auth_end < authority_begin)
		/* no auth info present, do nothing */
		return {};

	/* duplicate the full URI and then delete the auth
	   information */
	std::string result(uri);
	result.erase(authority_begin, auth_end + 1 - authority_begin);
	return result;
}

std::string
UriSquashDotSegments(std::string_view uri) noexcept
{
	std::forward_list<std::string_view> path = SplitString(std::string_view(uri), '/', false);
	path.remove_if([](const std::string_view &seg) { return seg == "."; });
	path.reverse();

	std::string result;

	unsigned segskips = 0;
	auto it = path.begin();
	while (it != path.end()) {
		if (*it == "..") {
			segskips++;
			it++;
			continue;
		} else if (segskips != 0) {
			segskips--;
			it++;
			continue;
		}

		if (!result.empty())
			result.insert(0, "/"sv);

		result.insert(0, *it);

		it++;
	}

	return result;
}
