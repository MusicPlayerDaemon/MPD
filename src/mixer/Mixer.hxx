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

private:
	/**
	 * This mutex protects all of the mixer struct, including its
	 * implementation, so plugins don't have to deal with that.
	 */
	Mutex mutex;

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

private:
	void _Open();
	void _Close() noexcept;

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
};
