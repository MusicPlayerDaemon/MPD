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

#ifndef MPD_OUTPUT_CONTROL_HXX
#define MPD_OUTPUT_CONTROL_HXX

#include "Source.hxx"
#include "pcm/AudioFormat.hxx"
#include "thread/Thread.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "system/PeriodClock.hxx"

#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <string>

enum class ReplayGainMode : uint8_t;
struct FilteredAudioOutput;
struct MusicChunk;
struct ConfigBlock;
class MusicPipe;
class Mixer;
class AudioOutputClient;

/**
 * Controller for an #AudioOutput and its output thread.
 */
class AudioOutputControl {
	std::unique_ptr<FilteredAudioOutput> output;

	/**
	 * A copy of FilteredAudioOutput::name which we need just in
	 * case this is a "dummy" output (output==nullptr) because
	 * this output has been moved to another partitioncommands.
	 */
	const std::string name;

	/**
	 * The PlayerControl object which "owns" this output.  This
	 * object is needed to signal command completion.
	 */
	AudioOutputClient &client;

	/**
	 * Source of audio data.
	 */
	AudioOutputSource source;

	/**
	 * The error that occurred in the output thread.  It is
	 * cleared whenever the output is opened successfully.
	 *
	 * Protected by #mutex.
	 */
	std::exception_ptr last_error;

	/**
	 * If not nullptr, the device has failed, and this timer is used
	 * to estimate how long it should stay disabled (unless
	 * explicitly reopened with "play").
	 */
	PeriodClock fail_timer;

	/**
	 * The thread handle, or nullptr if the output thread isn't
	 * running.
	 */
	Thread thread;

	/**
	 * This condition object wakes up the output thread after
	 * #command has been set.
	 */
	Cond wake_cond;

	/**
	 * This condition object signals #command completion to the
	 * client.
	 */
	Cond client_cond;

	/**
	 * Additional data for #command.  Protected by #mutex.
	 */
	struct Request {
		/**
		 * The #AudioFormat requested by #Command::OPEN.
		 */
		AudioFormat audio_format;

		/**
		 * The #MusicPipe passed to #Command::OPEN.
		 */
		const MusicPipe *pipe;
	} request;

	/**
	 * The next command to be performed by the output thread.
	 */
	enum class Command {
		NONE,
		ENABLE,
		DISABLE,

		/**
		 * Open the output, or reopen it if it is already
		 * open, adjusting for input #AudioFormat changes.
		 */
		OPEN,

		CLOSE,
		PAUSE,

		/**
		 * Close or pause the device, depending on the
		 * #always_on setting.
		 */
		RELEASE,

		/**
		 * Drains the internal (hardware) buffers of the device.  This
		 * operation may take a while to complete.
		 */
		DRAIN,

		CANCEL,
		KILL
	} command = Command::NONE;

	/**
	 * Will this output receive tags from the decoder?  The
	 * default is true, but it may be configured to false to
	 * suppress sending tags to the output.
	 */
	const bool tags;

	/**
	 * Shall this output always play something (i.e. silence),
	 * even when playback is stopped?
	 */
	const bool always_on;

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
	 * Is the device currently playing, i.e. is its buffer
	 * (likely) non-empty?  If not, then it will never be drained.
	 *
	 * This field is only valid while the output is open.
	 */
	bool playing;

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
	 * Was an #AudioOutputInterrupted caught?  In this case,
	 * playback is suspended, and the output thread waits for a
	 * command.
	 *
	 * This field is only valid while the output is open.
	 */
	bool caught_interrupted;

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

	/**
	 * If this flag is set, then the next WaitForDelay() call is
	 * skipped.  This is used to avoid delays after resuming
	 * playback.
	 */
	bool skip_delay;

	/**
	 * Has Command::KILL already been sent?  This field is only
	 * defined if `thread` is defined.  It shall avoid sending the
	 * command twice.
	 *
	 * Protected by #mutex.
	 */
	bool killed;

public:
	/**
	 * This mutex protects #open, #fail_timer, #pipe.
	 */
	mutable Mutex mutex;

	/**
	 * Throws on error.
	 */
	AudioOutputControl(std::unique_ptr<FilteredAudioOutput> _output,
			   AudioOutputClient &_client,
			   const ConfigBlock &block);

	/**
	 * Move the contents of an existing instance, and convert that
	 * existing instance to a "dummy" output.
	 */
	AudioOutputControl(AudioOutputControl &&src,
			   AudioOutputClient &_client) noexcept;

	~AudioOutputControl() noexcept;

	AudioOutputControl(const AudioOutputControl &) = delete;
	AudioOutputControl &operator=(const AudioOutputControl &) = delete;

	[[gnu::pure]]
	const char *GetName() const noexcept;

	[[gnu::pure]]
	const char *GetPluginName() const noexcept;

	[[gnu::pure]]
	const char *GetLogName() const noexcept;

	AudioOutputClient &GetClient() noexcept {
		return client;
	}

	[[gnu::pure]]
	Mixer *GetMixer() const noexcept;

	bool IsDummy() const noexcept {
		return !output;
	}

	/**
	 * Caller must lock the mutex.
	 */
	bool IsEnabled() const noexcept {
		return enabled;
	}

	/**
	 * @return true if the value has been modified
	 */
	bool LockSetEnabled(bool new_value) noexcept;

	/**
	 * @return the new "enabled" value
	 */
	bool LockToggleEnabled() noexcept;

	/**
	 * Caller must lock the mutex.
	 */
	bool IsOpen() const noexcept {
		return open;
	}

	/**
	 * Caller must lock the mutex.
	 */
	bool IsBusy() const noexcept {
		return IsOpen() && !IsCommandFinished();
	}

	/**
	 * Caller must lock the mutex.
	 */
	const std::exception_ptr &GetLastError() const noexcept {
		return last_error;
	}

	/**
	 * Detach and return the #FilteredAudioOutput instance and,
	 * replacing it here with a "dummy" object.
	 */
	std::unique_ptr<FilteredAudioOutput> Steal() noexcept;
	void ReplaceDummy(std::unique_ptr<FilteredAudioOutput> new_output,
			  bool _enabled) noexcept;

	void StartThread();

	/**
	 * Caller must lock the mutex.
	 */
	bool IsCommandFinished() const noexcept {
		return command == Command::NONE;
	}

	void CommandFinished() noexcept;

	/**
	 * Waits for command completion.
	 *
	 * Caller must lock the mutex.
	 */
	void WaitForCommand(std::unique_lock<Mutex> &lock) noexcept;

	void LockWaitForCommand() noexcept {
		std::unique_lock<Mutex> lock(mutex);
		WaitForCommand(lock);
	}

	/**
	 * Sends a command, but does not wait for completion.
	 *
	 * Caller must lock the mutex.
	 */
	void CommandAsync(Command cmd) noexcept;

	/**
	 * Sends a command to the object and waits for completion.
	 *
	 * Caller must lock the mutex.
	 */
	void CommandWait(std::unique_lock<Mutex> &lock, Command cmd) noexcept;

	/**
	 * Lock the object and execute the command synchronously.
	 */
	void LockCommandWait(Command cmd) noexcept;

	void BeginDestroy() noexcept;

	std::map<std::string, std::string> GetAttributes() const noexcept;
	void SetAttribute(std::string &&name, std::string &&value);

	/**
	 * Enables the device, but don't wait for completion.
	 *
	 * Caller must lock the mutex.
	 */
	void EnableAsync();

	/**
	 * Disables the device, but don't wait for completion.
	 *
	 * Caller must lock the mutex.
	 */
	void DisableAsync() noexcept;

	/**
	 * Attempt to enable or disable the device as specified by the
	 * #enabled attribute; attempt to sync it with #really_enabled
	 * (wrapper for EnableAsync() or DisableAsync()).
	 *
	 * Caller must lock the mutex.
	 */
	void EnableDisableAsync();

	void LockEnableDisableAsync() {
		const std::scoped_lock<Mutex> protect(mutex);
		EnableDisableAsync();
	}

	void LockPauseAsync() noexcept;

	void CloseWait(std::unique_lock<Mutex> &lock) noexcept;
	void LockCloseWait() noexcept;

	/**
	 * Closes the audio output, but if the "always_on" flag is set, put it
	 * into pause mode instead.
	 */
	void LockRelease() noexcept;

	void SetReplayGainMode(ReplayGainMode _mode) noexcept {
		source.SetReplayGainMode(_mode);
	}

	/**
	 * Caller must lock the mutex.
	 *
	 * Throws on error.
	 */
	void InternalOpen2(AudioFormat in_audio_format);

	/**
	 * Caller must lock the mutex.
	 */
	bool Open(std::unique_lock<Mutex> &lock,
		  AudioFormat audio_format, const MusicPipe &mp) noexcept;

	/**
	 * Opens or closes the device, depending on the "enabled"
	 * flag.
	 *
	 * @param force true to ignore the #fail_timer
	 * @return true if the device is open
	 */
	bool LockUpdate(const AudioFormat audio_format,
			const MusicPipe &mp,
			bool force) noexcept;

	/**
	 * Did we already consumed this chunk?
	 *
	 * Caller must lock the mutex.
	 */
	[[gnu::pure]]
	bool IsChunkConsumed(const MusicChunk &chunk) const noexcept;

	[[gnu::pure]]
	bool LockIsChunkConsumed(const MusicChunk &chunk) const noexcept;

	/**
	 * There's only one chunk left in the pipe (#pipe), and all
	 * audio outputs have consumed it already.  Clear the
	 * reference.
	 *
	 * This stalls playback to give the caller a chance to shift
	 * the #MusicPipe without getting disturbed; after this,
	 * LockAllowPlay() must be called to resume playback.
	 */
	void ClearTailChunk(const MusicChunk &chunk) noexcept {
		if (!IsOpen())
			return;

		source.ClearTailChunk(chunk);
		allow_play = false;
	}

	/**
	 * Locking wrapper for ClearTailChunk().
	 */
	void LockClearTailChunk(const MusicChunk &chunk) noexcept {
		const std::scoped_lock<Mutex> lock(mutex);
		ClearTailChunk(chunk);
	}

	void LockPlay() noexcept;
	void LockDrainAsync() noexcept;

	/**
	 * Clear the "allow_play" flag and send the "CANCEL" command
	 * asynchronously.  To finish the operation, the caller has to
	 * call LockAllowPlay().
	 */
	void LockCancelAsync() noexcept;

	/**
	 * Set the "allow_play" and signal the thread.
	 */
	void LockAllowPlay() noexcept;

private:
	/**
	 * An error has occurred and this output is defunct.
	 */
	void Failure(std::exception_ptr e) noexcept {
		last_error = e;

		/* don't automatically reopen this device for 10
		   seconds */
		fail_timer.Update();
	}

	/**
	 * Runs inside the OutputThread.
	 * Caller must lock the mutex.
	 * Handles exceptions.
	 *
	 * @return true on success
	 */
	bool InternalEnable() noexcept;

	/**
	 * Runs inside the OutputThread.
	 * Caller must lock the mutex.
	 */
	void InternalDisable() noexcept;

	/**
	 * Runs inside the OutputThread.
	 * Caller must lock the mutex.
	 * Handles exceptions.
	 */
	void InternalOpen(AudioFormat audio_format,
			  const MusicPipe &pipe) noexcept;

	/**
	 * Runs inside the OutputThread.
	 * Caller must lock the mutex.
	 */
	void InternalCloseOutput(bool drain) noexcept;

	/**
	 * Runs inside the OutputThread.
	 * Caller must lock the mutex.
	 */
	void InternalClose(bool drain) noexcept;

	/**
	 * An error has occurred, and this output must be closed.
	 */
	void InternalCloseError(std::exception_ptr e) noexcept {
		Failure(e);
		InternalClose(false);
	}

	/**
	 * Runs inside the OutputThread.
	 * Caller must lock the mutex.
	 */
	void InternalCheckClose(bool drain) noexcept;

	/**
	 * Wait until the output's delay reaches zero.
	 *
	 * @return true if playback should be continued, false if a
	 * command was issued
	 */
	bool WaitForDelay(std::unique_lock<Mutex> &lock) noexcept;

	/**
	 * Caller must lock the mutex.
	 */
	bool FillSourceOrClose() noexcept;

	/**
	 * Caller must lock the mutex.
	 */
	bool PlayChunk(std::unique_lock<Mutex> &lock) noexcept;

	/**
	 * Plays all remaining chunks, until the tail of the pipe has
	 * been reached (and no more chunks are queued), or until a
	 * command is received.
	 *
	 * Runs inside the OutputThread.
	 * Caller must lock the mutex.
	 * Handles exceptions.
	 *
	 * @return true if at least one chunk has been available,
	 * false if the tail of the pipe was already reached
	 */
	bool InternalPlay(std::unique_lock<Mutex> &lock) noexcept;

	/**
	 * Runs inside the OutputThread.
	 * Caller must lock the mutex.
	 * Handles exceptions.
	 */
	void InternalPause(std::unique_lock<Mutex> &lock) noexcept;

	/**
	 * Runs inside the OutputThread.
	 * Caller must lock the mutex.
	 * Handles exceptions.
	 */
	void InternalDrain() noexcept;

	void StopThread() noexcept;

	/**
	 * The OutputThread.
	 */
	void Task() noexcept;
};

#endif
