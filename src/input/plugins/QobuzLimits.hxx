// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "lib/curl/StringOptions.hxx"
#include "util/ByteSizes.hxx"

/**
 * Options for all responses from the Qobuz API.
 */
static constexpr Curl::StringOptions qobuz_string_options{
	.max_size = 1_MiB,
};
