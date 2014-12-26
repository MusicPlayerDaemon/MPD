/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include <assert.h>
#include <string.h>

bool uri_has_scheme(const char *uri)
{
	return strstr(uri, "://") != nullptr;
}

std::string
uri_get_scheme(const char *uri)
{
	const char *end = strstr(uri, "://");
	if (end == nullptr)
		end = uri;

	return std::string(uri, end);
}

/* suffixes should be ascii only characters */
const char *
uri_get_suffix(const char *uri)
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
uri_get_suffix(const char *uri, UriSuffixBuffer &buffer)
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
verify_uri_segment(const char *p)
{
	const char *q;

	unsigned dots = 0;
	while (*p == '.') {
		++p;
		++dots;
	}

	if (dots <= 2 && (*p == 0 || *p == '/'))
		return nullptr;

	q = strchr(p + 1, '/');
	return q != nullptr ? q : "";
}

bool
uri_safe_local(const char *uri)
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

std::string
uri_remove_auth(const char *uri)
{
	const char *auth, *slash, *at;

	if (memcmp(uri, "http://", 7) == 0)
		auth = uri + 7;
	else if (memcmp(uri, "https://", 8) == 0)
		auth = uri + 8;
	else if (memcmp(uri, "ftp://", 6) == 0)
		auth = uri + 6;
	else
		/* unrecognized URI */
		return std::string();

	slash = strchr(auth, '/');
	if (slash == nullptr)
		slash = auth + strlen(auth);

	at = (const char *)memchr(auth, '@', slash - auth);
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
uri_is_child(const char *parent, const char *child)
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
uri_is_child_or_same(const char *parent, const char *child)
{
	return strcmp(parent, child) == 0 || uri_is_child(parent, child);
}

std::string
uri_apply_base(const std::string &uri, const std::string &base)
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
