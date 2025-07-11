// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Headers.hxx"

#include <string>

struct StringCurlResponse {
	unsigned status;
	Curl::Headers headers;
	std::string body;
};
