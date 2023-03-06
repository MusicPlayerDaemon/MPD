// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
