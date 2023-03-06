// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef CURL_VERSION_HXX
#define CURL_VERSION_HXX

[[gnu::const]]
bool
IsCurlOlderThan(unsigned version_num) noexcept;

#endif
