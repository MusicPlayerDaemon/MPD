/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_FLAC_METADATA_H
#define MPD_FLAC_METADATA_H

#include "Compiler.h"
#include "FlacIOHandle.hxx"

#include <FLAC/metadata.h>

#include <assert.h>

struct tag_handler;
class MixRampInfo;

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

	bool Read(const char *path) {
		return ::FLAC__metadata_chain_read(chain, path);
	}

	bool Read(FLAC__IOHandle handle, FLAC__IOCallbacks callbacks) {
		return ::FLAC__metadata_chain_read_with_callbacks(chain,
								  handle,
								  callbacks);
	}

	bool Read(InputStream &is) {
		return Read(::ToFlacIOHandle(is), ::GetFlacIOCallbacks(is));
	}

	bool ReadOgg(const char *path) {
		return ::FLAC__metadata_chain_read_ogg(chain, path);
	}

	bool ReadOgg(FLAC__IOHandle handle, FLAC__IOCallbacks callbacks) {
		return ::FLAC__metadata_chain_read_ogg_with_callbacks(chain,
								      handle,
								      callbacks);
	}

	bool ReadOgg(InputStream &is) {
		return ReadOgg(::ToFlacIOHandle(is), ::GetFlacIOCallbacks(is));
	}

	gcc_pure
	FLAC__Metadata_ChainStatus GetStatus() const {
		return ::FLAC__metadata_chain_status(chain);
	}

	gcc_pure
	const char *GetStatusString() const {
		return FLAC__Metadata_ChainStatusString[GetStatus()];
	}

	void Scan(const tag_handler *handler, void *handler_ctx);
};

class FLACMetadataIterator {
	FLAC__Metadata_Iterator *iterator;

public:
	FLACMetadataIterator():iterator(::FLAC__metadata_iterator_new()) {}

	FLACMetadataIterator(FlacMetadataChain &chain)
		:iterator(::FLAC__metadata_iterator_new()) {
		::FLAC__metadata_iterator_init(iterator,
					       (FLAC__Metadata_Chain *)chain);
	}

	~FLACMetadataIterator() {
		::FLAC__metadata_iterator_delete(iterator);
	}

	bool Next() {
		return ::FLAC__metadata_iterator_next(iterator);
	}

	gcc_pure
	FLAC__StreamMetadata *GetBlock() {
		return ::FLAC__metadata_iterator_get_block(iterator);
	}
};

struct Tag;
struct ReplayGainInfo;

bool
flac_parse_replay_gain(ReplayGainInfo &rgi,
		       const FLAC__StreamMetadata_VorbisComment &vc);

MixRampInfo
flac_parse_mixramp(const FLAC__StreamMetadata_VorbisComment &vc);

Tag
flac_vorbis_comments_to_tag(const FLAC__StreamMetadata_VorbisComment *comment);

void
flac_scan_metadata(const FLAC__StreamMetadata *block,
		   const tag_handler *handler, void *handler_ctx);

#endif
