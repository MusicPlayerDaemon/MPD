// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <utility> // for std::exchange()

extern "C" {
#include <nfsc/libnfs.h>
}

/**
 * OO wrapper for #nfs_url.
 */
class NfsUrl {
	struct nfs_url *value = nullptr;

public:
	NfsUrl() noexcept = default;

	explicit NfsUrl(struct nfs_url *_value) noexcept
		:value(_value) {}

	NfsUrl(NfsUrl &&src) noexcept
		:value(std::exchange(src.value, nullptr)) {}

	~NfsUrl() noexcept {
		if (value != nullptr)
			nfs_destroy_url(value);
	}

	NfsUrl &operator=(NfsUrl &&src) noexcept {
		using std::swap;
		swap(value, src.value);
		return *this;
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
