// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "time/PeriodClock.hxx"

class MultipleOutputs;
class BufferedOutputStream;

/**
 * Cache for hardware/software volume levels.
 */
class MixerMemento {
	unsigned volume_software_set = 100;

	/** the cached hardware mixer value; invalid if negative */
	int last_hardware_volume = -1;

	/** the age of #last_hardware_volume */
	PeriodClock hardware_volume_clock;

public:
	/**
	 * Flush the hardware volume cache.
	 */
	void InvalidateHardwareVolume() noexcept {
		last_hardware_volume = -1;
	}

	[[gnu::pure]]
	int GetVolume(const MultipleOutputs &outputs) noexcept;

	/**
	 * Throws on error.
	 *
	 * Note: the caller is responsible for emitting #IDLE_MIXER.
	 */
	void SetVolume(MultipleOutputs &outputs, unsigned volume);

	bool LoadSoftwareVolumeState(const char *line, MultipleOutputs &outputs);

	void SaveSoftwareVolumeState(BufferedOutputStream &os) const;

	/**
	 * Generates a hash number for the current state of the software
	 * volume control.  This is used by timer_save_state_file() to
	 * determine whether the state has changed and the state file should
	 * be saved.
	 */
	[[gnu::pure]]
	unsigned GetSoftwareVolumeStateHash() const noexcept {
		return volume_software_set;
	}

private:
	bool SetSoftwareVolume(MultipleOutputs &outputs, unsigned volume);
	void SetHardwareVolume(MultipleOutputs &outputs, unsigned volume);
};
