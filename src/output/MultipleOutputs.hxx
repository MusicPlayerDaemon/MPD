// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef OUTPUT_ALL_H
#define OUTPUT_ALL_H

#include "MusicChunkPtr.hxx"
#include "player/Outputs.hxx"
#include "pcm/AudioFormat.hxx"
#include "thread/Mutex.hxx"
#include "Chrono.hxx"

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

enum class ReplayGainMode : uint8_t;
class MusicPipe;
class EventLoop;
class MixerListener;
class AudioOutputClient;
class AudioOutputControl;
struct ConfigData;
struct ReplayGainConfig;

/*
 * Wrap multiple #AudioOutputControl objects a single interface which
 * keeps them synchronized.
 */
class MultipleOutputs final : public PlayerOutputs {
	AudioOutputClient &client;

	MixerListener &mixer_listener;

	/**
	 * Protects #outputs.
	 *
	 * The main thread is allowed to read #outputs without holding
	 * the lock, because only the main thread is allowed to modify
	 * it.
	 */
	mutable Mutex mutex;

	std::vector<std::unique_ptr<AudioOutputControl>> outputs;

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
	MultipleOutputs(AudioOutputClient &_client,
			MixerListener &_mixer_listener) noexcept;
	~MultipleOutputs() noexcept;

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
	 *
	 * Since this returns an unprotected reference, it may only be
	 * called by the main thread (i.e. the only thread that is
	 * allowed to modify #outputs).
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
	 * Are all outputs dummy?
	 *
	 * May only be called by the main thread (i.e. the only thread
	 * that is allowed to modify #outputs).
	 */
	[[gnu::pure]]
	bool IsDummy() const noexcept;

	/**
	 * Returns the index of the audio output device with the specified name.
	 * Returns -1 if the name does not exist.
	 */
	[[gnu::pure]]
	int FindIndexByName(std::string_view name) const noexcept;

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

	/**
	 * Replace the output at the specified index with a dummy
	 * output and return the original output to the caller.
	 */
	[[nodiscard]]
	std::unique_ptr<AudioOutputControl> ReplaceWithDummy(std::size_t idx) noexcept;

	/**
	 * Replace the dummy output at the specified index with #src.
	 */
	void ReplaceDummy(std::size_t idx,
			  std::unique_ptr<AudioOutputControl> &&src,
			  bool enable,
			  ReplayGainMode replay_gain_mode) noexcept;

	void Add(std::unique_ptr<AudioOutputControl> &&src,
		 bool enable,
		 ReplayGainMode replay_gain_mode) noexcept;

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

#endif
