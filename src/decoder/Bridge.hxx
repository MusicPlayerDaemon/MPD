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

#ifndef MPD_DECODER_BRIDGE_HXX
#define MPD_DECODER_BRIDGE_HXX

#include "Client.hxx"
#include "tag/ReplayGainInfo.hxx"
#include "MusicChunkPtr.hxx"

#include <exception>
#include <memory>

class PcmConvert;
struct MusicChunk;
class DecoderControl;
class Path;
struct Tag;

/**
 * A bridge between the #DecoderClient interface and the MPD core
 * (#DecoderControl, #MusicPipe etc.).
 */
class DecoderBridge final : public DecoderClient {
public:
	DecoderControl &dc;

private:
	/**
	 * For converting input data to the configured audio format.
	 * nullptr means no conversion necessary.
	 */
	std::unique_ptr<PcmConvert> convert;

	/**
	 * The time stamp of the next data chunk, in seconds.
	 */
	FloatDuration timestamp = FloatDuration::zero();

	/**
	 * The time stamp of the next data chunk, in PCM frames.
	 */
	uint64_t absolute_frame = 0;

	/**
	 * Is the initial seek (to the start position of the sub-song)
	 * pending, or has it been performed already?
	 */
	bool initial_seek_pending;

	/**
	 * Are initial seek failures fatal?
	 */
	const bool initial_seek_essential;

	/**
	 * Is the initial seek currently running?  During this time,
	 * the decoder command is SEEK.  This flag is set by
	 * decoder_get_virtual_command(), when the virtual SEEK
	 * command is generated for the first time.
	 */
	bool initial_seek_running = false;

	/**
	 * This flag is set by GetSeekTime(), and checked by
	 * CommandFinished().  It is used to clean up after seeking.
	 */
	bool seeking = false;

	/**
	 * The tag from the song object.  This is only used for local
	 * files, because we expect the stream server to send us a new
	 * tag each time we play it.
	 */
	std::unique_ptr<Tag> song_tag;

public:
	/** the last tag received from the stream */
	std::unique_ptr<Tag> stream_tag;

	/** the last tag received from the decoder plugin */
	std::unique_ptr<Tag> decoder_tag;

private:
	/** the chunk currently being written to */
	MusicChunkPtr current_chunk;

	ReplayGainInfo replay_gain_info;

	/**
	 * A positive serial number for checking if replay gain info
	 * has changed since the last check.
	 */
	unsigned replay_gain_serial = 0;

	/**
	 * An error has occurred (in DecoderAPI.cxx), and the plugin
	 * will be asked to stop.
	 */
	std::exception_ptr error;

public:
	DecoderBridge(DecoderControl &_dc, bool _initial_seek_pending,
		      bool _initial_seek_essential,
		      std::unique_ptr<Tag> _tag) noexcept;

	~DecoderBridge() noexcept;

	void Reset() noexcept {
		error = {};
	}

	/**
	 * Should be read operation be cancelled?  That is the case when the
	 * player thread has sent a command such as "STOP".
	 *
	 * Caller must lock the #DecoderControl object.
	 */
	[[gnu::pure]]
	bool CheckCancelRead() const noexcept;

	/**
	 * Returns the current chunk the decoder writes to, or allocates a new
	 * chunk if there is none.
	 *
	 * @return the chunk, or NULL if we have received a decoder command
	 */
	MusicChunk *GetChunk() noexcept;

	/**
	 * Flushes the current chunk.
	 *
	 * Caller must not lock the #DecoderControl object.
	 */
	void FlushChunk() noexcept;

	void CheckFlushChunk() {
		if (current_chunk != nullptr)
			FlushChunk();
	}

	void CheckRethrowError() {
		if (error)
			std::rethrow_exception(error);
	}

	/**
	 * Open a local file.
	 */
	InputStreamPtr OpenLocal(Path path_fs, const char *uri_utf8);

	/* virtual methods from DecoderClient */
	void Ready(AudioFormat audio_format,
		   bool seekable, SignedSongTime duration) noexcept override;
	DecoderCommand GetCommand() noexcept override;
	void CommandFinished() noexcept override;
	SongTime GetSeekTime() noexcept override;
	uint64_t GetSeekFrame() noexcept override;
	void SeekError() noexcept override;
	InputStreamPtr OpenUri(const char *uri) override;
	size_t Read(InputStream &is,
		    void *buffer, size_t length) noexcept override;
	void SubmitTimestamp(FloatDuration t) noexcept override;
	DecoderCommand SubmitData(InputStream *is,
				  const void *data, size_t length,
				  uint16_t kbit_rate) noexcept override;
	DecoderCommand SubmitTag(InputStream *is, Tag &&tag) noexcept override;
	void SubmitReplayGain(const ReplayGainInfo *replay_gain_info) noexcept override;
	void SubmitMixRamp(MixRampInfo &&mix_ramp) noexcept override;

private:
	/**
	 * Checks if we need an "initial seek".  If so, then the
	 * initial seek is prepared, and the function returns true.
	 */
	bool PrepareInitialSeek() noexcept;

	/**
	 * Returns the current decoder command.  May return a
	 * "virtual" synthesized command, e.g. to seek to the
	 * beginning of the CUE track.
	 */
	DecoderCommand GetVirtualCommand() noexcept;
	DecoderCommand LockGetVirtualCommand() noexcept;

	/**
	 * Sends a #Tag as-is to the #MusicPipe.  Flushes the current
	 * chunk (DecoderBridge::chunk) if there is one.
	 */
	DecoderCommand DoSendTag(const Tag &tag) noexcept;

	bool UpdateStreamTag(InputStream *is) noexcept;
};

#endif
