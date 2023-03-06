// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FlacMetadataChain.hxx"
#include "FlacMetadataIterator.hxx"
#include "FlacIOHandle.hxx"
#include "FlacStreamMetadata.hxx"

bool
FlacMetadataChain::Read(InputStream &is) noexcept
{
	return Read(::ToFlacIOHandle(is), ::GetFlacIOCallbacks(is));
}

bool
FlacMetadataChain::ReadOgg(InputStream &is) noexcept
{
	return ReadOgg(::ToFlacIOHandle(is), ::GetFlacIOCallbacks(is));
}

void
FlacMetadataChain::Scan(TagHandler &handler) noexcept
{
	FlacMetadataIterator iterator(chain);

	do {
		FLAC__StreamMetadata *block = iterator.GetBlock();
		if (block == nullptr)
			break;

		flac_scan_metadata(block, handler);
	} while (iterator.Next());
}
