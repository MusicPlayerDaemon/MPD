// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

extern "C" {
#include <nfsc/libnfs.h>
}

/**
 * OO wrapper for #nfs_url.
 */
class NfsUrl {
	struct nfs_url *const value;

public:
	explicit NfsUrl(struct nfs_url *_value) noexcept
		:value(_value) {}

	~NfsUrl() noexcept {
		nfs_destroy_url(value);
	}

	NfsUrl(const NfsUrl &) = delete;
	NfsUrl &operator=(const NfsUrl &) = delete;

	const struct nfs_url *operator->() const noexcept {
		return value;
	}

	const struct nfs_url &operator*() const noexcept {
		return *value;
	}
};
