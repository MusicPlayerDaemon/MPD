/*
 * Copyright 2003-2019 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "UriUtil.hxx"
#include "ASCII.hxx"
#include "CharUtil.hxx"

#include <assert.h>
#include <string.h>

static constexpr bool
IsValidSchemeStart(char ch)
{
	return IsLowerAlphaASCII(ch);
}

static constexpr bool
IsValidSchemeChar(char ch)
{
	return IsLowerAlphaASCII(ch) || IsDigitASCII(ch) ||
		ch == '+' || ch == '.' || ch == '-';
}

gcc_pure
static bool
IsValidScheme(StringView p) noexcept
{
	if (p.empty() || !IsValidSchemeStart(p.front()))
		return false;

	for (size_t i = 1; i < p.size; ++i)
		if (!IsValidSchemeChar(p[i]))
			return false;

	return true;
}

/**
 * Return the URI part after the scheme specification (and after the
 * double slash).
 */
gcc_pure
static const char *
uri_after_scheme(const char *uri) noexcept
{
	if (uri[0] == '/' && uri[1] == '/' && uri[2] != '/')
		return uri + 2;

	const char *colon = strchr(uri, ':');
	return colon != nullptr &&
		IsValidScheme({uri, colon}) &&
		colon[1] == '/' && colon[2] == '/'
		? colon + 3
		: nullptr;
}

bool
uri_has_scheme(const char *uri) noexcept
{
	return strstr(uri, "://") != nullptr;
}

std::string
uri_get_scheme(const char *uri) noexcept
{
	const char *end = strstr(uri, "://");
	if (end == nullptr)
		end = uri;

	return std::string(uri, end);
}

const char *
uri_get_path(const char *uri) noexcept
{
	const char *ap = uri_after_scheme(uri);
	if (ap != nullptr)
		return strchr(ap, '/');

	return uri;
}

/* suffixes should be ascii only characters */
const char *
uri_get_suffix(const char *uri) noexcept
{
	const char *suffix = strrchr(uri, '.');
	if (suffix == nullptr || suffix == uri ||
	    suffix[-1] == '/' || suffix[-1] == '\\')
		return nullptr;

	++suffix;

	if (strpbrk(suffix, "/\\") != nullptr)
		return nullptr;

	return suffix;
}

const char *
uri_get_suffix(const char *uri, UriSuffixBuffer &buffer) noexcept
{
	const char *suffix = uri_get_suffix(uri);
	if (suffix == nullptr)
		return nullptr;

	const char *q = strchr(suffix, '?');
	if (q != nullptr && size_t(q - suffix) < sizeof(buffer.data)) {
		memcpy(buffer.data, suffix, q - suffix);
		buffer.data[q - suffix] = 0;
		suffix = buffer.data;
	}

	return suffix;
}

static const char *
verify_uri_segment(const char *p) noexcept
{
	unsigned dots = 0;
	while (*p == '.') {
		++p;
		++dots;
	}

	if (dots <= 2 && (*p == 0 || *p == '/'))
		return nullptr;

	const char *q = strchr(p + 1, '/');
	return q != nullptr ? q : "";
}

bool
uri_safe_local(const char *uri) noexcept
{
	while (true) {
		uri = verify_uri_segment(uri);
		if (uri == nullptr)
			return false;

		if (*uri == 0)
			return true;

		assert(*uri == '/');

		++uri;
	}
}

gcc_pure
static const char *
SkipUriScheme(const char *uri) noexcept
{
	const char *const schemes[] = { "http://", "https://", "ftp://" };
	for (auto scheme : schemes) {
		auto result = StringAfterPrefixCaseASCII(uri, scheme);
		if (result != nullptr)
			return result;
	}

	return nullptr;
}

std::string
uri_remove_auth(const char *uri) noexcept
{
	const char *auth = SkipUriScheme(uri);
	if (auth == nullptr)
		/* unrecognized URI */
		return std::string();

	const char *slash = strchr(auth, '/');
	if (slash == nullptr)
		slash = auth + strlen(auth);

	const char *at = (const char *)memchr(auth, '@', slash - auth);
	if (at == nullptr)
		/* no auth info present, do nothing */
		return std::string();

	/* duplicate the full URI and then delete the auth
	   information */
	std::string result(uri);
	result.erase(auth - uri, at + 1 - auth);
	return result;
}

bool
uri_is_child(const char *parent, const char *child) noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(parent != nullptr);
	assert(child != nullptr);
#endif

	const size_t parent_length = strlen(parent);
	return memcmp(parent, child, parent_length) == 0 &&
		child[parent_length] == '/';
}


bool
uri_is_child_or_same(const char *parent, const char *child) noexcept
{
	return strcmp(parent, child) == 0 || uri_is_child(parent, child);
}

std::string
uri_apply_base(const std::string &uri, const std::string &base) noexcept
{
	if (uri.front() == '/') {
		/* absolute path: replace the whole URI path in base */

		auto i = base.find("://");
		if (i == base.npos)
			/* no scheme: override base completely */
			return uri;

		/* find the first slash after the host part */
		i = base.find('/', i + 3);
		if (i == base.npos)
			/* there's no URI path - simply append uri */
			i = base.length();

		return base.substr(0, i) + uri;
	}

	std::string out(base);
	if (out.back() != '/')
		out.push_back('/');

	out += uri;
	return out;
}
