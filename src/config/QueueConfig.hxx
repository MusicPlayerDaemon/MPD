// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

struct QueueConfig {
	static constexpr unsigned DEFAULT_MAX_LENGTH = 16 * 1024;
	static constexpr unsigned MAX_MAX_LENGTH = 16 * 1024 * 1024;

	unsigned max_length = DEFAULT_MAX_LENGTH;
};
