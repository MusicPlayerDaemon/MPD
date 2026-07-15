// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "MusicChunkPtr.hxx"
#include "player/Outputs.hxx"
#include "pcm/AudioFormat.hxx"
#include "thread/Mutex.hxx"
#include "util/IntrusiveList.hxx"
#include "Chrono.hxx"

#include <cassert>
#include <concepts>
#include <cstdint>
#include <memory>
#include <vector>

enum class ReplayGainMode : uint8_t;
class MusicPipe;
class MixerListener;
class AllOutputs;
class AudioOutputClient;
class AudioOutputControl;

/*
 * Wrap multiple #AudioOutputControl objects a single interface which
 * keeps them synchronized.
 */
class MultipleOutputs final : public PlayerOutputs {
	AllOutputs &all_outputs;

	AudioOutputClient &client;

	MixerListener &mixer_listener;

	/**
	 * Protects #output.
	 *
	 * The main thread is allowed to read #per_output without
	 * holding the lock, because only the main thread is allowed
	 * to modify it.
	 */
	mutable Mutex mutex;

	/**
	 * A doubly-linked list of outputs owned by this object.
	 */
	IntrusiveList<AudioOutputControl> outputs;

	AudioFormat input_audio_format = AudioFormat::Undefined();

	/**
	 * The #MusicPipe object which feeds all audio outputs.  It is
	 * filled by Play().
	 */
	std::unique_ptr<MusicPipe> pipe;

	/**
	 * The "elapsed_time" stamp of the most recently finished
	 * chunk.
	 */
	SignedSongTime elapsed_time = SignedSongTime::Negative();

public:
	/**
	 * Load audio outputs from the configuration file and
	 * initialize them.
	 */
	MultipleOutputs(AllOutputs &_all_outputs, AudioOutputClient &_client,
			MixerListener &_mixer_listener) noexcept;
	~MultipleOutputs() noexcept;

	const auto &GetAllOutputs() const noexcept {
		return all_outputs;
	}

	bool empty() const noexcept {
		return outputs.empty();
	}

	auto begin() const noexcept {
		return outputs.begin();
	}

	auto end() const noexcept {
		return outputs.end();
	}

	/**
	 * Returns the audio output device with the specified name.
	 * Returns nullptr if the name does not exist.
	 */
	[[gnu::pure]]
	AudioOutputControl *FindByName(std::string_view name) noexcept;

	/**
	 * Does this object own the specified #AudioOutputControl instance?
	 *
	 * May only be called from the main thread.
	 */
	[[gnu::pure]]
	bool Owns(const AudioOutputControl &ao) const noexcept;

	/**
	 * Acquire ownership of all (orphan) outputs in #all_outputs
	 * (but do not enable/disable them).  This is what we do with
	 * the default partitions on startup.
	 *
	 * This method is unsafe for later use because it does not
	 * care for mutex locking.
	 */
	void AcquireAll(ReplayGainMode replay_gain_mode) noexcept;

	void AcquireOwnership(AudioOutputControl &ao, bool enable,
			      ReplayGainMode replay_gain_mode) noexcept;
	void ReleaseOwnership(AudioOutputControl &ao) noexcept;

	void SetReplayGainMode(ReplayGainMode mode) noexcept;

	/**
	 * Returns the average volume of all available mixers (range
	 * 0..100).  Returns -1 if no mixer can be queried.
	 */
	[[gnu::pure]]
	int GetVolume() const noexcept;

	/**
	 * Sets the volume on all available mixers.
	 *
	 * Throws on error.
	 *
	 * @param volume the volume (range 0..100)
	 */
	void SetVolume(unsigned volume);

	/**
	 * Similar to GetVolume(), but gets the volume only for
	 * software mixers.  See #software_mixer_plugin.  This
	 * function fails if no software mixer is configured.
	 */
	[[gnu::pure]]
	int GetSoftwareVolume() const noexcept;

	/**
	 * Similar to SetVolume(), but sets the volume only for
	 * software mixers.  See #software_mixer_plugin.  This
	 * function cannot fail, because the underlying software
	 * mixers cannot fail either.
	 */
	void SetSoftwareVolume(unsigned volume) noexcept;

private:
	/**
	 * Was Open() called successfully?
	 *
	 * This method may only be called from the player thread.
	 */
	bool IsOpen() const noexcept {
		return input_audio_format.IsDefined();
	}

	/**
	 * Wait until all (active) outputs have finished the current
	 * command.
	 *
	 * Caller must lock #mutex.
	 */
	void WaitAll() noexcept;

	/**
	 * Signals all audio outputs which are open.
	 *
	 * Caller must lock #mutex.
	 */
	void AllowPlay() noexcept;

	/**
	 * A version of _EnableDisable() that expects the caller to
	 * lock #mutex.
	 */
	bool _Update(bool force) noexcept;

	/**
	 * Opens all output devices which are enabled, but closed.
	 *
	 * @return true if there is at least open output device which
	 * is open
	 */
	bool Update(bool force) noexcept;

	/**
	 * Has this chunk been consumed by all audio outputs?
	 */
	bool IsChunkConsumed(const MusicChunk *chunk) const noexcept;

	/**
	 * A version of EnableDisable() that expects the caller to
	 * lock #mutex.
	 */
	void _EnableDisable();

	/**
	 * A version of Close() that expects the caller to lock
	 * #mutex.
	 */
	void _Close() noexcept;

	/* virtual methods from class PlayerOutputs */
	void EnableDisable() override;
	void Open(AudioFormat audio_format) override;
	void Close() noexcept override;
	void Release() noexcept override;
	void Play(MusicChunkPtr chunk) override;
	unsigned CheckPipe() noexcept override;
	void Pause() noexcept override;
	void Drain() noexcept override;
	void Cancel() noexcept override;
	void SongBorder() noexcept override;
	SignedSongTime GetElapsedTime() const noexcept override {
		return elapsed_time;
	}
};
