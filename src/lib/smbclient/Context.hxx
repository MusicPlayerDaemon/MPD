/*
 * Copyright 2003-2020 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_SMBCLIENT_CONTEXT_HXX
#define MPD_SMBCLIENT_CONTEXT_HXX

#include <libsmbclient.h>

#include <utility>

/**
 * Wrapper for `SMBCCTX*`.
 */
class SmbclientContext {
	SMBCCTX *ctx = nullptr;

	explicit SmbclientContext(SMBCCTX *_ctx) noexcept
		:ctx(_ctx) {}

public:
	SmbclientContext() = default;

	~SmbclientContext() noexcept {
		if (ctx != nullptr)
			smbc_free_context(ctx, 1);
	}

	SmbclientContext(SmbclientContext &&src) noexcept
		:ctx(std::exchange(src.ctx, nullptr)) {}

	SmbclientContext &operator=(SmbclientContext &&src) noexcept {
		using std::swap;
		swap(ctx, src.ctx);
		return *this;
	}

	/**
	 * Throws on error.
	 */
	static SmbclientContext New();
};

#endif
