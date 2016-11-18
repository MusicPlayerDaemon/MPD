/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include "config.h"
#include "DecoderInternal.hxx"
#include "DecoderControl.hxx"
#include "pcm/PcmConvert.hxx"
#include "MusicPipe.hxx"
#include "MusicBuffer.hxx"
#include "MusicChunk.hxx"
#include "tag/Tag.hxx"

#include <assert.h>

Decoder::~Decoder()
{
	/* caller must flush the chunk */
	assert(current_chunk == nullptr);

	if (convert != nullptr) {
		convert->Close();
		delete convert;
	}

	delete song_tag;
	delete stream_tag;
	delete decoder_tag;
}

/**
 * All chunks are full of decoded data; wait for the player to free
 * one.
 */
static DecoderCommand
need_chunks(DecoderControl &dc)
{
	if (dc.command == DecoderCommand::NONE)
		dc.Wait();

	return dc.command;
}

static DecoderCommand
LockNeedChunks(DecoderControl &dc)
{
	const ScopeLock protect(dc.mutex);
	return need_chunks(dc);
}

MusicChunk *
Decoder::GetChunk()
{
	DecoderCommand cmd;

	if (current_chunk != nullptr)
		return current_chunk;

	do {
		current_chunk = dc.buffer->Allocate();
		if (current_chunk != nullptr) {
			current_chunk->replay_gain_serial = replay_gain_serial;
			if (replay_gain_serial != 0)
				current_chunk->replay_gain_info = replay_gain_info;

			return current_chunk;
		}

		cmd = LockNeedChunks(dc);
	} while (cmd == DecoderCommand::NONE);

	return nullptr;
}

void
Decoder::FlushChunk()
{
	assert(!seeking);
	assert(!initial_seek_running);
	assert(!initial_seek_pending);
	assert(current_chunk != nullptr);

	auto *chunk = std::exchange(current_chunk, nullptr);
	if (chunk->IsEmpty())
		dc.buffer->Return(chunk);
	else
		dc.pipe->Push(chunk);

	const ScopeLock protect(dc.mutex);
	if (dc.client_is_waiting)
		dc.client_cond.signal();
}
