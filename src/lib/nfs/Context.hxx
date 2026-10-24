// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <utility> // for std::exchange()

struct nfs_context;
class NfsUrl;

/**
 * OO wrapper for #nfs_context.
 */
class NfsContext {
	struct nfs_context *value = nullptr;

public:
	constexpr NfsContext() noexcept = default;

	struct Create {};

	/**
	 * Create a new #nfs_context.
	 *
	 * Throws on error.
	 */
	NfsContext(Create);

	NfsContext(NfsContext &&src) noexcept
		:value(std::exchange(src.value, nullptr)) {}

	~NfsContext() noexcept {
		if (value != nullptr)
			reset();
	}

	NfsContext &operator=(NfsContext &&src) noexcept {
		using std::swap;
		swap(value, src.value);
		return *this;
	}

	operator struct nfs_context *() const noexcept {
		return value;
	}

	void reset() noexcept;

	NfsUrl ParseUrlDir(const char *url) const;
};
