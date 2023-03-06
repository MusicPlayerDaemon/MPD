// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef CURL_FORM_HXX
#define CURL_FORM_HXX

#include "Headers.hxx"

#include <curl/curl.h>

#include <string>

/**
 * Encode the given map of form fields to a
 * "application/x-www-form-urlencoded" string.
 */
std::string
EncodeForm(CURL *curl, const Curl::Headers &fields) noexcept;

#endif
