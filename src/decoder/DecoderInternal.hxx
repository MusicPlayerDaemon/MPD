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

#ifndef MPD_DECODER_INTERNAL_HXX
#define MPD_DECODER_INTERNAL_HXX

#include "ReplayGainInfo.hxx"
#include "util/Error.hxx"

class PcmConvert;
struct MusicChunk;
struct DecoderControl;
struct Tag;

struct Decoder {
	DecoderControl &dc;

	/**
	 * For converting input data to the configured audio format.
	 * nullptr means no conversion necessary.
	 */
	PcmConvert *convert;

	/**
	 * The time stamp of the next data chunk, in seconds.
	 */
	double timestamp;

	/**
	 * Is the initial seek (to the start position of the sub-song)
	 * pending, or has it been performed already?
	 */
	bool initial_seek_pending;

	/**
	 * Is the initial seek currently running?  During this time,
	 * the decoder command is SEEK.  This flag is set by
	 * decoder_get_virtual_command(), when the virtual SEEK
	 * command is generated for the first time.
	 */
	bool initial_seek_running;

	/**
	 * This flag is set by decoder_seek_time(), and checked by
	 * decoder_command_finished().  It is used to clean up after
	 * seeking.
	 */
	bool seeking;

	/**
	 * The tag from the song object.  This is only used for local
	 * files, because we expect the stream server to send us a new
	 * tag each time we play it.
	 */
	Tag *song_tag;

	/** the last tag received from the stream */
	Tag *stream_tag;

	/** the last tag received from the decoder plugin */
	Tag *decoder_tag;

	/** the chunk currently being written to */
	MusicChunk *chunk;

	ReplayGainInfo replay_gain_info;

	/**
	 * A positive serial number for checking if replay gain info
	 * has changed since the last check.
	 */
	unsigned replay_gain_serial;

	/**
	 * An error has occurred (in DecoderAPI.cxx), and the plugin
	 * will be asked to stop.
	 */
	Error error;

	Decoder(DecoderControl &_dc, bool _initial_seek_pending, Tag *_tag)
		:dc(_dc),
		 convert(nullptr),
		 timestamp(0),
		 initial_seek_pending(_initial_seek_pending),
		 initial_seek_running(false),
		 seeking(false),
		 song_tag(_tag), stream_tag(nullptr), decoder_tag(nullptr),
		 chunk(nullptr),
		 replay_gain_serial(0) {
	}

	~Decoder();

	/**
	 * Returns the current chunk the decoder writes to, or allocates a new
	 * chunk if there is none.
	 *
	 * @return the chunk, or NULL if we have received a decoder command
	 */
	MusicChunk *GetChunk();

	/**
	 * Flushes the current chunk.
	 *
	 * Caller must not lock the #DecoderControl object.
	 */
	void FlushChunk();
};

#endif
