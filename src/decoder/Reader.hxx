// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DECODER_READER_HXX
#define MPD_DECODER_READER_HXX

#include "io/Reader.hxx"

class DecoderClient;
class InputStream;

/**
 * A wrapper for decoder_read() which implements the #Reader
 * interface.
 */
class DecoderReader final : public Reader {
	DecoderClient &client;
	InputStream &is;

public:
	DecoderReader(DecoderClient &_client, InputStream &_is)
		:client(_client), is(_is) {}

	DecoderClient &GetClient() {
		return client;
	}

	InputStream &GetInputStream() {
		return is;
	}

	/* virtual methods from class Reader */
	std::size_t Read(std::span<std::byte> dest) override;
};

#endif
