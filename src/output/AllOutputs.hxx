// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <cassert>
#include <memory>
#include <vector>

struct ConfigData;
struct ReplayGainConfig;
class EventLoop;
class AudioOutputControl;

/**
 * Manages the global list of #AudioOutputControl instances.
 *
 * This class only owns those objects, but #MultipleOutputs is the one
 * that will drive them.
 */
class AllOutputs final {
	std::vector<std::unique_ptr<AudioOutputControl>> outputs;

public:
	AllOutputs() noexcept;
	~AllOutputs() noexcept;

	void Configure(EventLoop &event_loop, EventLoop &rt_event_loop,
		       const ConfigData &config,
		       const ReplayGainConfig &replay_gain_config);

	/**
	 * Returns the total number of audio output devices, including
	 * those which are disabled right now.
	 */
	[[gnu::pure]]
	std::size_t Size() const noexcept {
		return outputs.size();
	}

	/**
	 * Returns the "i"th audio output device.
	 */
	const AudioOutputControl &Get(std::size_t i) const noexcept {
		assert(i < Size());

		return *outputs[i];
	}

	AudioOutputControl &Get(std::size_t i) noexcept {
		assert(i < Size());

		return *outputs[i];
	}

	/**
	 * Returns the audio output device with the specified name.
	 * Returns nullptr if the name does not exist.
	 */
	[[gnu::pure]]
	AudioOutputControl *FindByName(std::string_view name) noexcept;

	/**
	 * Does an audio output device with this name exist?
	 */
	[[gnu::pure]]
	bool HasName(std::string_view name) noexcept {
		return FindByName(name) != nullptr;
	}
};
