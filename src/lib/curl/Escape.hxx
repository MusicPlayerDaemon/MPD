// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <curl/curl.h>

#include <string>
#include <string_view>

std::string
CurlEscapeUriPath(std::string_view src) noexcept;

std::string
CurlUnescape(std::string_view src) noexcept;
