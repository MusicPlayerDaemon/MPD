// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "UriUtil.hxx"
#include "ASCII.hxx"
#include "SplitString.hxx"

#include <array>
#include <cassert>
#include <cstring>

#include <string_view>

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

	const char *q = std::strchr(p + 1, '/');
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

[[gnu::pure]]
static const char *
SkipUriScheme(const char *uri) noexcept
{
	static constexpr auto schemes = std::array {
		"http://", "https://",
		"ftp://",
		"smb://",
	};

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
		return {};

	const char *slash = std::strchr(auth, '/');
	if (slash == nullptr)
		slash = auth + strlen(auth);

	auto at = (const char *)std::memchr(auth, '@', slash - auth);
	if (at == nullptr)
		/* no auth info present, do nothing */
		return {};

	/* duplicate the full URI and then delete the auth
	   information */
	std::string result(uri);
	result.erase(auth - uri, at + 1 - auth);
	return result;
}

std::string
uri_squash_dot_segments(const char *uri) noexcept
{
	std::forward_list<std::string_view> path = SplitString(std::string_view(uri), '/', false);
	path.remove_if([](const std::string_view &seg) { return seg == "."; });
	path.reverse();

	std::string result;

	int segskips = 0;
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

		result.insert(0, *it);

		if (it != path.begin()) {
			result.insert(it->length(), "/");
		}

		it++;
	}

	return result;
}
