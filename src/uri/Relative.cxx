// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Relative.hxx"
#include "Extract.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"
#include "util/Compiler.h"

#include <cassert>

#include <string.h>

using std::string_view_literals::operator""sv;

bool
uri_is_child(const char *parent, const char *child) noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(parent != nullptr);
	assert(child != nullptr);
#endif

	const char *suffix = StringAfterPrefix(child, parent);
	return suffix != nullptr && *suffix != 0 &&
		(suffix == child || suffix[-1] == '/' || *suffix == '/');
}


bool
uri_is_child_or_same(const char *parent, const char *child) noexcept
{
	return StringIsEqual(parent, child) || uri_is_child(parent, child);
}

std::string
uri_apply_base(std::string_view uri, std::string_view base) noexcept
{
	if (uri.front() == '/') {
		/* absolute path: replace the whole URI path in base */

		auto i = base.find("://");
		if (i == base.npos)
			/* no scheme: override base completely */
			return std::string(uri);

		/* find the first slash after the host part */
		i = base.find('/', i + 3);
		if (i == base.npos)
			/* there's no URI path - simply append uri */
			i = base.length();

		std::string out(base.substr(0, i));
		out += uri;
		return out;
	}

	std::string out(base);
	if (out.back() != '/')
		out.push_back('/');

	out += uri;
	return out;
}

/**
 * Return the URI path without the last segment (but leave the
 * trailing slash).
 */
static constexpr std::string_view
UriPathWithoutFilename(std::string_view path) noexcept
{
	const auto slash = path.rfind('/');
	if (slash != path.npos)
		return path.substr(0, slash + 1);
	else
		return path.substr(0, 0);
}

static void
StripLeadingSlashes(std::string_view &s) noexcept
{
	while (s.starts_with('/'))
		s.remove_prefix(1);
}

/**
 * Return the URI path (ending with a slash) without the last segment
 * (still ending with a slash).  May return an empty string no slash
 * remains.
 */
static constexpr std::string_view
UriPathWithoutLastSegment(std::string_view path) noexcept
{
	assert(!path.empty());
	assert(path.back() == '/');

	path.remove_suffix(1);
	return UriPathWithoutFilename(path);
}

static bool
ConsumeSpecial(std::string_view &relative_path, std::string_view &base_path) noexcept
{
	while (true) {
		if (SkipPrefix(relative_path, "./"sv)) {
			StripLeadingSlashes(relative_path);
		} else if (SkipPrefix(relative_path, "../"sv)) {
			StripLeadingSlashes(relative_path);

			if (base_path.size() <= 1)
				/* base_path is either already empty
				   or consists of a single slash: we
				   can't strip the last segment,
				   therefore fail */
				return false;

			base_path = UriPathWithoutLastSegment(base_path);

			/* if base_path did not start with a slash, it
			   may now be empty */
		} else if (relative_path == "."sv) {
			relative_path.remove_prefix(1);
			return true;
		} else
			return true;
	}
}

std::string
uri_apply_relative(std::string_view relative_uri,
		   std::string_view base_uri) noexcept
{
	if (relative_uri.empty())
		return std::string(base_uri);

	if (uri_has_scheme(relative_uri))
		return std::string(relative_uri);

	// TODO: support double slash at beginning of relative_uri
	if (relative_uri.front() == '/') {
		/* absolute path: replace the whole URI path in base */

		auto i = base_uri.find("://");
		if (i == base_uri.npos)
			/* no scheme: override base completely */
			return std::string{relative_uri};

		/* find the first slash after the host part */
		i = base_uri.find('/', i + 3);
		if (i == base_uri.npos)
			/* there's no URI path - simply append uri */
			i = base_uri.length();

		std::string result{base_uri.substr(0, i)};
		result.append(relative_uri);
		return result;
	}

	std::string_view relative_path{relative_uri};

	const auto _base_path = uri_get_path(base_uri);
	if (_base_path.data() == nullptr) {
		std::string result(base_uri);
		if (relative_path.front() != '/')
			result.push_back('/');
		while (SkipPrefix(relative_path, "./"sv)) {}
		if (relative_path.starts_with("../"sv))
			return {};
		if (relative_path != "."sv)
			result += relative_path;
		return result;
	}

	std::string_view base_path = UriPathWithoutFilename(_base_path);

	if (!ConsumeSpecial(relative_path, base_path))
		return {};

	std::string result(base_uri.data(), _base_path.data());
	result.append(base_path);
	result.append(relative_path);
	return result;
}
