// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "InputPlugin.hxx"
#include "util/StringCompare.hxx"

#include <algorithm>
#include <cassert>
#include <iterator>

bool
InputPlugin::SupportsUri(const char *uri) const noexcept
{
	assert(prefixes || protocols);
	if (prefixes != nullptr) {
		for (auto i = prefixes; *i != nullptr; ++i)
			if (StringStartsWithIgnoreCase(uri, *i))
				return true;
	} else {
		const auto p = protocols();
		return std::any_of(p.begin(), p.end(), [uri](const auto &schema)
			{ return StringStartsWithIgnoreCase(uri, schema.c_str()); } );
	}
	return false;
}

// Note: The whitelist has to be ordered alphabetically
constexpr static const char *whitelist[] = {
	"ftp",
	"ftps",
	"gopher",
	"http",
	"https",
	"mmsh",
	"mmst",
	"rtmp",
	"rtmpe",
	"rtmps",
	"rtmpt",
	"rtmpte",
	"rtmpts",
	"rtp",
	"scp",
	"sftp",
	"smb",
	"srtp",
};

bool
protocol_is_whitelisted(const char *proto) noexcept
{
	auto begin = std::begin(whitelist);
	auto end = std::end(whitelist);
	return std::binary_search(begin, end, proto, [](const char* a, const char* b) {
		return strcasecmp(a,b) < 0;
	});
}
