/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_FLAC_METADATA_CHAIN_HXX
#define MPD_FLAC_METADATA_CHAIN_HXX

#include <FLAC/metadata.h>

class InputStream;
class TagHandler;

class FlacMetadataChain {
	FLAC__Metadata_Chain *chain;

public:
	FlacMetadataChain():chain(::FLAC__metadata_chain_new()) {}

	~FlacMetadataChain() {
		::FLAC__metadata_chain_delete(chain);
	}

	explicit operator FLAC__Metadata_Chain *() {
		return chain;
	}

	bool Read(const char *path) noexcept {
		return ::FLAC__metadata_chain_read(chain, path);
	}

	bool Read(FLAC__IOHandle handle,
		  FLAC__IOCallbacks callbacks) noexcept {
		return ::FLAC__metadata_chain_read_with_callbacks(chain,
								  handle,
								  callbacks);
	}

	bool Read(InputStream &is) noexcept;

	bool ReadOgg(const char *path) noexcept {
		return ::FLAC__metadata_chain_read_ogg(chain, path);
	}

	bool ReadOgg(FLAC__IOHandle handle,
		     FLAC__IOCallbacks callbacks) noexcept {
		return ::FLAC__metadata_chain_read_ogg_with_callbacks(chain,
								      handle,
								      callbacks);
	}

	bool ReadOgg(InputStream &is) noexcept;

	[[gnu::pure]]
	FLAC__Metadata_ChainStatus GetStatus() const noexcept {
		return ::FLAC__metadata_chain_status(chain);
	}

	[[gnu::pure]]
	const char *GetStatusString() const noexcept {
		return FLAC__Metadata_ChainStatusString[GetStatus()];
	}

	void Scan(TagHandler &handler) noexcept;
};

#endif
