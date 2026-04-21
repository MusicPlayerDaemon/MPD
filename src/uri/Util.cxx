// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Util.hxx"
#include "util/ASCII.hxx"
#include "util/SplitString.hxx"
#include "util/StringCompare.hxx"
#include "util/StringListVerify.hxx"

#include <array>
#include <string_view>

using std::string_view_literals::operator""sv;

bool
uri_safe_local(std::string_view uri) noexcept
{
	return IsNonEmptyListOf(uri, '/', [](std::string_view segment){
		return !segment.empty() && segment != "."sv && segment != ".."sv;
	});
}

[[gnu::pure]]
static std::string_view
SkipUriScheme(std::string_view uri) noexcept
{
	static constexpr auto schemes = std::array {
		"http://", "https://",
		"ftp://",
		"smb://",
	};

	for (const std::string_view scheme : schemes) {
		if (SkipPrefixIgnoreCase(uri, scheme))
			return uri;
	}

	return {};
}

std::string
uri_remove_auth(const std::string_view uri) noexcept
{
	const std::string_view after_uri_scheme = SkipUriScheme(uri);
	if (after_uri_scheme.data() == nullptr)
		/* unrecognized URI */
		return {};

	std::size_t path_start = after_uri_scheme.find('/');
	if (path_start == after_uri_scheme.npos)
		path_start = after_uri_scheme.size();

	const auto [host_and_auth, path] = Partition(after_uri_scheme, path_start);

	auto [_, host] = Split(host_and_auth, '@');
	if (host.data() == nullptr)
		/* no auth info present, do nothing */
		return {};

	/* duplicate the full URI and then delete the auth
	   information */
	std::string result(uri);
	result.erase(std::distance(uri.begin(), after_uri_scheme.begin()),
		     std::distance(after_uri_scheme.begin(), host.begin()));
	return result;
}

std::string
uri_squash_dot_segments(std::string_view uri) noexcept
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
