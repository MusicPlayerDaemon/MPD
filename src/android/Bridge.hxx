// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "event/InjectEvent.hxx"

#include <atomic>

struct Instance;

/**
 * Executes the requests from the JNI functions (which are called from
 * other threads) in the main thread.  While an object of this class
 * exists, the JNI functions control its #Instance; the destructor
 * unregisters it, so the JNI functions never access a destroyed
 * #Instance.
 */
class AndroidControl {
	Instance &instance;

	InjectEvent inject_event;

	std::atomic_bool pause{false}, next{false}, previous{false};
	std::atomic_uint play_pause{0};

public:
	explicit AndroidControl(Instance &_instance) noexcept;
	~AndroidControl() noexcept;

	AndroidControl(const AndroidControl &) = delete;
	AndroidControl &operator=(const AndroidControl &) = delete;

	void Shutdown() noexcept;

	void Pause() noexcept {
		pause = true;
		inject_event.Schedule();
	}

	void PlayPause() noexcept {
		++play_pause;
		inject_event.Schedule();
	}

	void PlayNext() noexcept {
		next = true;
		inject_event.Schedule();
	}

	void PlayPrevious() noexcept {
		previous = true;
		inject_event.Schedule();
	}

private:
	void OnInject() noexcept;
};
