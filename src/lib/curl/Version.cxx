// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Version.hxx"

#include <curl/curl.h>

bool
IsCurlOlderThan(unsigned version_num) noexcept
{
	const auto *const info = curl_version_info(CURLVERSION_FIRST);
	return info == nullptr || info->version_num < version_num;
}
