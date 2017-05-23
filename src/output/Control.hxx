/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "AudioFormat.hxx"
#include "thread/Thread.hxx"
#include "thread/Cond.hxx"
#include "system/PeriodClock.hxx"
#include "Compiler.h"

#include <utility>
#include <exception>

#ifndef NDEBUG
#include <assert.h>
#endif

#include <stdint.h>

enum class ReplayGainMode : uint8_t;
struct AudioOutput;
struct MusicChunk;
struct ConfigBlock;
class MusicPipe;
class Mutex;
class Mixer;
class AudioOutputClient;

/**
 * Controller for an #AudioOutput and its output thread.
 */
class AudioOutputControl {
	AudioOutput *output;

	/**
	 * The PlayerControl object which "owns" this output.  This
	 * object is needed to signal command completion.
	 */
	AudioOutputClient &client;

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
	Cond cond;

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

	/**
	 * If this flag is set, then the next WaitForDelay() call is
	 * skipped.  This is used to avoid delays after resuming
	 * playback.
	 */
	bool skip_delay;

public:
	Mutex &mutex;

	AudioOutputControl(AudioOutput *_output, AudioOutputClient &_client);

#ifndef NDEBUG
	~AudioOutputControl() {
		assert(!fail_timer.IsDefined());
		assert(!thread.IsDefined());
		assert(output == nullptr);
	}
#endif

	AudioOutputControl(const AudioOutputControl &) = delete;
	AudioOutputControl &operator=(const AudioOutputControl &) = delete;

	/**
	 * Throws #std::runtime_error on error.
	 */
	void Configure(const ConfigBlock &block);

	gcc_pure
	const char *GetName() const noexcept;

	AudioOutputClient &GetClient() noexcept {
		return client;
	}

	gcc_pure
	Mixer *GetMixer() const noexcept;

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

	gcc_pure
	bool IsOpen() const noexcept;

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

	void StartThread();
	void StopThread() noexcept;

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
	void WaitForCommand() noexcept;

	/**
	 * Sends a command, but does not wait for completion.
	 *
	 * Caller must lock the mutex.
	 */
	void CommandAsync(Command cmd) noexcept;

	/**
	 * Sends a command to the #AudioOutput object and waits for
	 * completion.
	 *
	 * Caller must lock the mutex.
	 */
	void CommandWait(Command cmd) noexcept;

	/**
	 * Lock the #AudioOutput object and execute the command
	 * synchronously.
	 */
	void LockCommandWait(Command cmd) noexcept;

	void BeginDestroy() noexcept;
	void FinishDestroy() noexcept;

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
	void LockPauseAsync() noexcept;

	void CloseWait() noexcept;
	void LockCloseWait() noexcept;

	/**
	 * Closes the audio output, but if the "always_on" flag is set, put it
	 * into pause mode instead.
	 */
	void LockRelease() noexcept;

	void SetReplayGainMode(ReplayGainMode _mode) noexcept;

	/**
	 * Caller must lock the mutex.
	 */
	bool Open(AudioFormat audio_format, const MusicPipe &mp) noexcept;

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

	gcc_pure
	bool LockIsChunkConsumed(const MusicChunk &chunk) const noexcept;

	void ClearTailChunk(const MusicChunk &chunk) noexcept;

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
	 * Runs inside the OutputThread.  Handles exceptions.
	 */
	void InternalOpen(AudioFormat audio_format,
			  const MusicPipe &pipe) noexcept;

	/**
	 * Wait until the output's delay reaches zero.
	 *
	 * @return true if playback should be continued, false if a
	 * command was issued
	 */
	bool WaitForDelay() noexcept;

	bool FillSourceOrClose();

	bool PlayChunk() noexcept;

	/**
	 * Plays all remaining chunks, until the tail of the pipe has
	 * been reached (and no more chunks are queued), or until a
	 * command is received.
	 *
	 * Runs inside the OutputThread.  Handles exceptions.
	 *
	 * @return true if at least one chunk has been available,
	 * false if the tail of the pipe was already reached
	 */
	bool InternalPlay() noexcept;

	/**
	 * Runs inside the OutputThread.  Handles exceptions.
	 */
	void InternalPause() noexcept;

	/**
	 * The OutputThread.
	 */
	void Task();
};

#endif
