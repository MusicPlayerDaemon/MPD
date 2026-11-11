// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

/**
 * A workaround for rounding errors when the resolution of a mixer
 * knob isn't fine enough to represent all 101 possible values
 * (0..100).
 *
 * It remembers the percent value passed to Mixer::SetVolume() (the
 * "desired" volume) and the volume which was actually set (the
 * "resulting" volume), i.e. the value which would be read back from
 * the hardware.
 *
 * As long as the hardware still has the "resulting" volume, we're
 * still on the value which was previously set (but may have been
 * rounded down or up), and the "desired" volume is reported
 * instead.
 */
class DesiredVolume {
	int desired = -1, resulting = -1;

public:
	/**
	 * Forget the volume passed to Set().
	 */
	constexpr void Clear() noexcept {
		desired = resulting = -1;
	}

	/**
	 * Remember the volume passed to Mixer::SetVolume() and the
	 * (rounded) volume which was actually set.
	 */
	constexpr void Set(int _desired, int _resulting) noexcept {
		desired = _desired;
		resulting = _resulting;
	}

	/**
	 * Is the specified volume (read from the hardware) still the
	 * one which was set by Set()?
	 */
	constexpr bool IsCurrent(int volume) const noexcept {
		return resulting >= 0 && volume == resulting;
	}

	/**
	 * Translate a volume read from the hardware: if it is still
	 * the one which was set by Set(), return the desired volume
	 * instead.
	 */
	constexpr int Translate(int volume) const noexcept {
		return IsCurrent(volume) ? desired : volume;
	}
};
