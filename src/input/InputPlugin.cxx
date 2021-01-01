/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
