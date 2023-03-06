// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "MimeType.hxx"
#include "IterableSplitString.hxx"
#include "util/StringSplit.hxx"
#include "util/StringStrip.hxx"

std::string_view
GetMimeTypeBase(std::string_view s) noexcept
{
	return Split(s, ';').first;
}

std::map<std::string, std::string>
ParseMimeTypeParameters(std::string_view mime_type) noexcept
{
	/* discard the first segment (the base MIME type) */
	const auto params = Split(mime_type, ';').second;

	std::map<std::string, std::string> result;
	for (const std::string_view i : IterableSplitString(params, ';')) {
		const auto s = Split(Strip(i), '=');
		if (!s.first.empty() && s.second.data() != nullptr)
			result.emplace(s);
	}

	return result;
}
