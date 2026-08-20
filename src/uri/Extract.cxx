// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Extract.hxx"
#include "util/CharUtil.hxx"
#include "util/StringSplit.hxx"
#include "util/StringVerify.hxx"

#include <cstring>

static constexpr bool
IsValidSchemeStart(char ch) noexcept
{
	return IsLowerAlphaASCII(ch);
}

static constexpr bool
IsValidSchemeChar(char ch) noexcept
{
	return IsLowerAlphaASCII(ch) || IsDigitASCII(ch) ||
		ch == '+' || ch == '.' || ch == '-';
}

[[gnu::pure]]
static bool
IsValidScheme(std::string_view p) noexcept
{
	if (p.empty() || !IsValidSchemeStart(p.front()))
		return false;

	return CheckChars(p.substr(1), IsValidSchemeChar);
}

bool
UriHasScheme(std::string_view uri) noexcept
{
	return !UriGetScheme(uri).empty();
}

std::string_view
UriGetScheme(std::string_view uri) noexcept
{
	auto end = uri.find("://");
	if (end == std::string_view::npos)
		return {};

	return uri.substr(0, end);
}

std::string_view
UriAfterScheme(std::string_view uri) noexcept
{
	if (uri.size() > 2 && uri[0] == '/' && uri[1] == '/' && uri[2] != '/')
		return uri.substr(2);

	const auto [scheme, rest] = Split(uri, ':');
	if (IsValidScheme(scheme) &&
	    rest.size() > 2 && rest[0] == '/' && rest[1] == '/' &&
	    rest[2] != '/')
		return rest.substr(2);

	return {};
}

bool
UriIsRelativePath(const char *uri) noexcept
{
	return !UriHasScheme(uri) && *uri != '/';
}

std::string_view
UriPathQueryFragment(std::string_view uri) noexcept
{
	if (std::string_view ap = UriAfterScheme(uri); ap.data() != nullptr) {
		auto slash = ap.find('/');
		if (slash == std::string_view::npos)
			return {};
		return ap.substr(slash);
	}

	return uri;
}

[[gnu::pure]]
static std::string_view
UriWithoutQueryString(std::string_view uri) noexcept
{
	return Split(uri, '?').first;
}

std::string_view
UriGetPath(std::string_view uri) noexcept
{
	auto path = UriPathQueryFragment(uri);
	if (path.data() == nullptr || path.data() == uri.data())
		/* preserve query and fragment if this URI doesn't
		   have a scheme; the question mark may be part of the
		   file name, after all */
		return path;

	auto end = path.find('?');
	if (end == std::string_view::npos)
		end = path.find('#');

	return path.substr(0, end);
}

/* suffixes should be ascii only characters */
std::string_view
UriGetSuffix(std::string_view _uri) noexcept
{
	const auto uri = UriWithoutQueryString(_uri);

	const auto dot = uri.rfind('.');
	if (dot == uri.npos || dot == 0 ||
	    uri[dot - 1] == '/' || uri[dot - 1] == '\\')
		return {};

	auto suffix = uri.substr(dot + 1);
	if (suffix.find('/') != suffix.npos ||
	    suffix.find('\\') != suffix.npos)
		/* this was not the last path segment */
		return {};

	return suffix;
}

const char *
UriGetFragment(const char *uri) noexcept
{
	const char *fragment = std::strchr(uri, '#');
	if (fragment == nullptr)
		return nullptr;

	return fragment + 1;
}
