// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Form.hxx"
#include "String.hxx"

std::string
EncodeForm(CURL *curl, const Curl::Headers &fields) noexcept
{
	std::string result;

	for (const auto &[key, field] : fields) {
		if (!result.empty())
			result.push_back('&');

		result.append(key);
		result.push_back('=');

		if (!field.empty()) {
			CurlString value(
				curl_easy_escape(curl, field.data(), field.length()));
			if (value)
				result.append(value);
		}
	}

	return result;
}
