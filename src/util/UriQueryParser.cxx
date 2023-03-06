// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "UriQueryParser.hxx"
#include "IterableSplitString.hxx"

using std::string_view_literals::operator""sv;

std::string_view
UriFindRawQueryParameter(std::string_view query_string, std::string_view name) noexcept
{
	for (const std::string_view i : IterableSplitString(query_string, '&')) {
		if (i.starts_with(name)) {
			if (i.size() == name.size())
				return ""sv;

			if (i[name.size()] == '=')
				return i.substr(name.size() + 1);
		}
	}

	return {};
}
