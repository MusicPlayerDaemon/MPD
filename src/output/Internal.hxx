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

#ifndef MPD_OUTPUT_INTERNAL_HXX
#define MPD_OUTPUT_INTERNAL_HXX

#include "AudioFormat.hxx"
#include "pcm/PcmBuffer.hxx"
#include "pcm/PcmDither.hxx"
#include "ReplayGainMode.hxx"
#include "filter/Observer.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "thread/Thread.hxx"
#include "system/PeriodClock.hxx"

class PreparedFilter;
class Filter;
class MusicPipe;
class EventLoop;
class Mixer;
class MixerListener;
struct MusicChunk;
struct ConfigBlock;
struct PlayerControl;
struct AudioOutputPlugin;
struct ReplayGainConfig;

struct AudioOutput {
	enum class Command {
		NONE,
		ENABLE,
		DISABLE,
		OPEN,

		/**
		 * This command is invoked when the input audio format
		 * changes.
		 */
		REOPEN,

		CLOSE,
		PAUSE,

		/**
		 * Drains the internal (hardware) buffers of the device.  This
		 * operation may take a while to complete.
		 */
		DRAIN,

		CANCEL,
		KILL
	};

	/**
	 * The device's configured display name.
	 */
	const char *name;

	/**
	 * The plugin which implements this output device.
	 */
	const AudioOutputPlugin &plugin;

	/**
	 * The #mixer object associated with this audio output device.
	 * May be nullptr if none is available, or if software volume is
	 * configured.
	 */
	Mixer *mixer = nullptr;

	/**
	 * Will this output receive tags from the decoder?  The
	 * default is true, but it may be configured to false to
	 * suppress sending tags to the output.
	 */
	bool tags;

	/**
	 * Shall this output always play something (i.e. silence),
	 * even when playback is stopped?
	 */
	bool always_on;

	/**
	 * Has the user enabled this device?
	 */
	bool enabled = true;

	/**
	 * Is this device actually enabled, i.e. the "enable" method
	 * has succeeded?
	 */
	bool really_enabled = false;

	/**
	 * Is the device (already) open and functional?
	 *
	 * This attribute may only be modified by the output thread.
	 * It is protected with #mutex: write accesses inside the
	 * output thread and read accesses outside of it may only be
	 * performed while the lock is held.
	 */
	bool open = false;

	/**
	 * Is the device paused?  i.e. the output thread is in the
	 * ao_pause() loop.
	 */
	bool pause = false;

	/**
	 * When this flag is set, the output thread will not do any
	 * playback.  It will wait until the flag is cleared.
	 *
	 * This is used to synchronize the "clear" operation on the
	 * shared music pipe during the CANCEL command.
	 */
	bool allow_play = true;

	/**
	 * True while the OutputThread is inside ao_play().  This
	 * means the PlayerThread does not need to wake up the
	 * OutputThread when new chunks are added to the MusicPipe,
	 * because the OutputThread is already watching that.
	 */
	bool in_playback_loop = false;

	/**
	 * Has the OutputThread been woken up to play more chunks?
	 * This is set by audio_output_play() and reset by ao_play()
	 * to reduce the number of duplicate wakeups.
	 */
	bool woken_for_play = false;

	ReplayGainMode replay_gain_mode = ReplayGainMode::OFF;

	/**
	 * If not nullptr, the device has failed, and this timer is used
	 * to estimate how long it should stay disabled (unless
	 * explicitly reopened with "play").
	 */
	PeriodClock fail_timer;

	/**
	 * The configured audio format.
	 */
	AudioFormat config_audio_format;

	/**
	 * The audio_format in which audio data is received from the
	 * player thread (which in turn receives it from the decoder).
	 */
	AudioFormat in_audio_format;

	/**
	 * The audio_format which is really sent to the device.  This
	 * is basically config_audio_format (if configured) or
	 * in_audio_format, but may have been modified by
	 * plugin->open().
	 */
	AudioFormat out_audio_format;

	/**
	 * The buffer used to allocate the cross-fading result.
	 */
	PcmBuffer cross_fade_buffer;

	/**
	 * The dithering state for cross-fading two streams.
	 */
	PcmDither cross_fade_dither;

	/**
	 * The filter object of this audio output.  This is an
	 * instance of chain_filter_plugin.
	 */
	PreparedFilter *prepared_filter = nullptr;
	Filter *filter_instance = nullptr;

	/**
	 * The #VolumeFilter instance of this audio output.  It is
	 * used by the #SoftwareMixer.
	 */
	FilterObserver volume_filter;

	/**
	 * The replay_gain_filter_plugin instance of this audio
	 * output.
	 */
	PreparedFilter *prepared_replay_gain_filter = nullptr;
	Filter *replay_gain_filter_instance = nullptr;

	/**
	 * The serial number of the last replay gain info.  0 means no
	 * replay gain info was available.
	 */
	unsigned replay_gain_serial;

	/**
	 * The replay_gain_filter_plugin instance of this audio
	 * output, to be applied to the second chunk during
	 * cross-fading.
	 */
	PreparedFilter *prepared_other_replay_gain_filter = nullptr;
	Filter *other_replay_gain_filter_instance = nullptr;

	/**
	 * The serial number of the last replay gain info by the
	 * "other" chunk during cross-fading.
	 */
	unsigned other_replay_gain_serial;

	/**
	 * The convert_filter_plugin instance of this audio output.
	 * It is the last item in the filter chain, and is responsible
	 * for converting the input data into the appropriate format
	 * for this audio output.
	 */
	FilterObserver convert_filter;

	/**
	 * The thread handle, or nullptr if the output thread isn't
	 * running.
	 */
	Thread thread;

	/**
	 * The next command to be performed by the output thread.
	 */
	Command command = Command::NONE;

	/**
	 * The music pipe which provides music chunks to be played.
	 */
	const MusicPipe *pipe;

	/**
	 * This mutex protects #open, #fail_timer, #current_chunk and
	 * #current_chunk_finished.
	 */
	Mutex mutex;

	/**
	 * This condition object wakes up the output thread after
	 * #command has been set.
	 */
	Cond cond;

	/**
	 * The PlayerControl object which "owns" this output.  This
	 * object is needed to signal command completion.
	 */
	PlayerControl *player_control;

	/**
	 * The #MusicChunk which is currently being played.  All
	 * chunks before this one may be returned to the
	 * #music_buffer, because they are not going to be used by
	 * this output anymore.
	 */
	const MusicChunk *current_chunk;

	/**
	 * Has the output finished playing #current_chunk?
	 */
	bool current_chunk_finished;

	/**
	 * Throws #std::runtime_error on error.
	 */
	AudioOutput(const AudioOutputPlugin &_plugin,
		    const ConfigBlock &block);

	~AudioOutput();

private:
	void Configure(const ConfigBlock &block);

public:
	void StartThread();
	void StopThread();

	void Finish();

	bool IsOpen() const {
		return open;
	}

	bool IsCommandFinished() const {
		return command == Command::NONE;
	}

	/**
	 * Waits for command completion.
	 *
	 * Caller must lock the mutex.
	 */
	void WaitForCommand();

	/**
	 * Sends a command, but does not wait for completion.
	 *
	 * Caller must lock the mutex.
	 */
	void CommandAsync(Command cmd);

	/**
	 * Sends a command to the #AudioOutput object and waits for
	 * completion.
	 *
	 * Caller must lock the mutex.
	 */
	void CommandWait(Command cmd);

	/**
	 * Lock the #AudioOutput object and execute the command
	 * synchronously.
	 */
	void LockCommandWait(Command cmd);

	/**
	 * Enables the device.
	 */
	void LockEnableWait();

	/**
	 * Disables the device.
	 */
	void LockDisableWait();

	void LockPauseAsync();

	/**
	 * Same LockCloseWait(), but expects the lock to be
	 * held by the caller.
	 */
	void CloseWait();
	void LockCloseWait();

	/**
	 * Closes the audio output, but if the "always_on" flag is set, put it
	 * into pause mode instead.
	 */
	void LockRelease();

	void SetReplayGainMode(ReplayGainMode _mode) {
		replay_gain_mode = _mode;
	}

	/**
	 * Caller must lock the mutex.
	 */
	bool Open(const AudioFormat audio_format, const MusicPipe &mp);

	/**
	 * Opens or closes the device, depending on the "enabled"
	 * flag.
	 *
	 * @return true if the device is open
	 */
	bool LockUpdate(const AudioFormat audio_format,
			const MusicPipe &mp);

	void LockPlay();

	void LockDrainAsync();

	/**
	 * Clear the "allow_play" flag and send the "CANCEL" command
	 * asynchronously.  To finish the operation, the caller has to
	 * call LockAllowPlay().
	 */
	void LockCancelAsync();

	/**
	 * Set the "allow_play" and signal the thread.
	 */
	void LockAllowPlay();

	/**
	 * Did we already consumed this chunk?
	 *
	 * Caller must lock the mutex.
	 */
	gcc_pure
	bool IsChunkConsumed(const MusicChunk &chunk) const;

	gcc_pure
	bool LockIsChunkConsumed(const MusicChunk &chunk) {
		const ScopeLock protect(mutex);
		return IsChunkConsumed(chunk);
	}

private:
	void CommandFinished();

	bool Enable();
	void Disable();

	void Open();
	void Close(bool drain);
	void Reopen();

	/**
	 * Close the output plugin.
	 *
	 * Mutex must not be locked.
	 */
	void CloseOutput(bool drain);

	/**
	 * Throws std::runtime_error on error.
	 */
	AudioFormat OpenFilter(AudioFormat &format);

	/**
	 * Mutex must not be locked.
	 */
	void CloseFilter();

	void ReopenFilter();

	/**
	 * Wait until the output's delay reaches zero.
	 *
	 * @return true if playback should be continued, false if a
	 * command was issued
	 */
	bool WaitForDelay();

	gcc_pure
	const MusicChunk *GetNextChunk() const;

	bool PlayChunk(const MusicChunk *chunk);

	/**
	 * Plays all remaining chunks, until the tail of the pipe has
	 * been reached (and no more chunks are queued), or until a
	 * command is received.
	 *
	 * @return true if at least one chunk has been available,
	 * false if the tail of the pipe was already reached
	 */
	bool Play();

	void Pause();

	/**
	 * The OutputThread.
	 */
	void Task();
	static void Task(void *arg);
};

/**
 * Notify object used by the thread's client, i.e. we will send a
 * notify signal to this object, expecting the caller to wait on it.
 */
extern struct notify audio_output_client_notify;

/**
 * Throws #std::runtime_error on error.
 */
AudioOutput *
audio_output_new(EventLoop &event_loop,
		 const ReplayGainConfig &replay_gain_config,
		 const ConfigBlock &block,
		 MixerListener &mixer_listener,
		 PlayerControl &pc);

void
audio_output_free(AudioOutput *ao);

#endif
