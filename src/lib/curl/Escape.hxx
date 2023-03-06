// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef CURL_ESCAPE_HXX
#define CURL_ESCAPE_HXX

#include <curl/curl.h>

#include <string>
#include <string_view>

std::string
CurlEscapeUriPath(CURL *curl, std::string_view src) noexcept;

std::string
CurlEscapeUriPath(std::string_view src) noexcept;

std::string
CurlUnescape(CURL *curl, std::string_view src) noexcept;

std::string
CurlUnescape(std::string_view src) noexcept;

#endif
