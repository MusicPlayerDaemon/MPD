// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Reader.hxx"
#include "DecoderAPI.hxx"

size_t
DecoderReader::Read(void *data, size_t size)
{
	return decoder_read(client, is, data, size);
}
