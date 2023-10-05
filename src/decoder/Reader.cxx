// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Reader.hxx"
#include "DecoderAPI.hxx"

std::size_t
DecoderReader::Read(std::span<std::byte> dest)
{
	return decoder_read(client, is, dest.data(), dest.size());
}
