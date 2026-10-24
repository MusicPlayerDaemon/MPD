// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "MixerPlugin.hxx"
#include "thread/Mutex.hxx"

#include <exception>

class MixerListener;

class Mixer {
	const MixerPlugin &plugin;

public:
	/* this field needs to be public for the workaround in
	   ReplayGainFilter::Update() - TODO eliminate this kludge */
	MixerListener &listener;

protected:
	/**
	 * This mutex protects all of the mixer struct, including its
	 * implementation, so plugins don't have to deal with that.
	 */
	Mutex mutex;

private:
	/**
	 * Contains error details if this mixer has failed.  If set,
	 * it should not be reopened automatically.
	 */
	std::exception_ptr failure;

	/**
	 * Is the mixer device currently open?
	 */
	bool open = false;

public:
	explicit Mixer(const MixerPlugin &_plugin,
		       MixerListener &_listener) noexcept
		:plugin(_plugin), listener(_listener) {}

	Mixer(const Mixer &) = delete;

	virtual ~Mixer() = default;

	bool IsPlugin(const MixerPlugin &other) const noexcept {
		return &plugin == &other;
	}

	bool IsGlobal() const noexcept {
		return plugin.global;
	}

	/**
	 * Throws on error.
	 */
	void LockOpen();

	void LockClose() noexcept;

	/**
	 * Close the mixer unless the plugin's "global" flag is set.
	 * This is called when the #AudioOutput is closed.
	 */
	void LockAutoClose() noexcept {
		if (!IsGlobal())
			LockClose();
	}

	/**
	 * Throws on error.
	 */
	int LockGetVolume();

	/**
	 * Throws on error.
	 */
	void LockSetVolume(unsigned volume);

	/**
	 * Set the volume to the given base volume multiplied with
	 * the given (linear) scale factor; see ScaleVolume().  This
	 * is used by the replay gain filter.
	 *
	 * Throws on error.
	 *
	 * @param base the volume (0..100 including) which
	 * corresponds to scale=1
	 * @param scale the linear scale factor (non-negative)
	 * @return the new volume (0..100 including) or -1 if the
	 * mixer is not open
	 */
	int LockSetScaledVolume(unsigned base, float scale);

private:
	void _Open();
	void _Close() noexcept;

	/**
	 * Try to ensure that the mixer is open.  If it is not, call
	 * _Open()
	 *
	 * Throws on error.
	 *
	 * @param rethrow if this #Mixer has failed prevously, rethrow
	 * that error; if this parameter is false, just return false
	 * without rethrowing
	 *
	 * @return true if the mixer is now open, false if not
	 * (because the associated output is closed)
	 */
	bool EnsureOpen(bool rethrow);

protected:
	/**
	 * Open mixer device
	 *
	 * Caller must lock the mutex.
	 *
	 * Throws std::runtime_error on error.
	 */
	virtual void Open() = 0;

	/**
	 * Close mixer device
	 *
	 * Caller must lock the mutex.
	 */
	virtual void Close() noexcept = 0;

	/**
	 * Reads the current volume.
	 *
	 * Caller must lock the mutex.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @return the current volume (0..100 including) or -1 if
	 * unavailable
	 */
	virtual int GetVolume() = 0;

	/**
	 * Sets the volume.
	 *
	 * Caller must lock the mutex.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param volume the new volume (0..100 including)
	 */
	virtual void SetVolume(unsigned volume) = 0;

	/**
	 * Calculate the volume which attenuates (or amplifies) the
	 * signal by the given linear factor relative to the given
	 * volume.  The result depends on the volume curve of the
	 * mixer.
	 *
	 * The default implementation assumes that the volume is
	 * proportional to the linear amplitude.
	 *
	 * Caller must lock the mutex.  The mixer is open.
	 *
	 * @param volume the volume (0..100 including) which
	 * corresponds to scale=1
	 * @param scale the linear scale factor (non-negative)
	 * @return the new volume (0..100 including)
	 */
	[[gnu::pure]]
	virtual unsigned ScaleVolume(unsigned volume, float scale) noexcept;
};
