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

#include "Compiler.h"

#include <utility>
#include <exception>

#ifndef NDEBUG
#include <assert.h>
#endif

#include <stdint.h>

enum class ReplayGainMode : uint8_t;
struct AudioFormat;
struct AudioOutput;
struct MusicChunk;
class MusicPipe;
class Mutex;
class Mixer;
class AudioOutputClient;

/**
 * Controller for an #AudioOutput and its output thread.
 */
class AudioOutputControl {
	AudioOutput *output;

public:
	Mutex &mutex;

	explicit AudioOutputControl(AudioOutput *_output);

#ifndef NDEBUG
	~AudioOutputControl() {
		assert(output == nullptr);
	}
#endif

	AudioOutputControl(const AudioOutputControl &) = delete;
	AudioOutputControl &operator=(const AudioOutputControl &) = delete;

	gcc_pure
	const char *GetName() const;

	AudioOutputClient &GetClient();

	gcc_pure
	Mixer *GetMixer() const;

	gcc_pure
	bool IsEnabled() const;

	/**
	 * @return true if the value has been modified
	 */
	bool LockSetEnabled(bool new_value);

	/**
	 * @return the new "enabled" value
	 */
	bool LockToggleEnabled();

	gcc_pure
	bool IsOpen() const;

	gcc_pure
	bool IsBusy() const;

	/**
	 * Caller must lock the mutex.
	 */
	gcc_const
	const std::exception_ptr &GetLastError() const;

	void WaitForCommand();

	void BeginDestroy();
	void FinishDestroy();

	void EnableDisableAsync();
	void LockPauseAsync();

	void LockCloseWait();
	void LockRelease();

	void SetReplayGainMode(ReplayGainMode _mode);

	/**
	 * Opens or closes the device, depending on the "enabled"
	 * flag.
	 *
	 * @param force true to ignore the #fail_timer
	 * @return true if the device is open
	 */
	bool LockUpdate(const AudioFormat audio_format,
			const MusicPipe &mp,
			bool force);

	gcc_pure
	bool LockIsChunkConsumed(const MusicChunk &chunk) const;

	void ClearTailChunk(const MusicChunk &chunk);

	void LockPlay();
	void LockDrainAsync();
	void LockCancelAsync();
	void LockAllowPlay();
};

#endif
