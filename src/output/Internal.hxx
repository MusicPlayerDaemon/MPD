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

#ifndef MPD_OUTPUT_INTERNAL_HXX
#define MPD_OUTPUT_INTERNAL_HXX

#include "Source.hxx"
#include "AudioFormat.hxx"
#include "filter/Observer.hxx"
#include "thread/Mutex.hxx"

class PreparedFilter;
class MusicPipe;
class EventLoop;
class Mixer;
class MixerListener;
class AudioOutputClient;
struct MusicChunk;
struct ConfigBlock;
struct AudioOutputPlugin;
struct ReplayGainConfig;

struct AudioOutput {
	/**
	 * The device's configured display name.
	 */
	const char *name;

	/**
	 * The plugin which implements this output device.
	 */
	const AudioOutputPlugin &plugin;

	/**
	 * The #mixer object associated with this audio output device.
	 * May be nullptr if none is available, or if software volume is
	 * configured.
	 */
	Mixer *mixer = nullptr;

	/**
	 * Is this device actually enabled, i.e. the "enable" method
	 * has succeeded?
	 */
	bool really_enabled = false;

	/**
	 * Is the device (already) open and functional?
	 *
	 * This attribute may only be modified by the output thread.
	 * It is protected with #mutex: write accesses inside the
	 * output thread and read accesses outside of it may only be
	 * performed while the lock is held.
	 */
	bool open = false;

	/**
	 * Is the device paused?  i.e. the output thread is in the
	 * ao_pause() loop.
	 */
	bool pause = false;

	/**
	 * The configured audio format.
	 */
	AudioFormat config_audio_format;

	/**
	 * The #AudioFormat which is emitted by the #Filter, with
	 * #config_audio_format already applied.  This is used to
	 * decide whether this object needs to be closed and reopened
	 * upon #AudioFormat changes.
	 */
	AudioFormat filter_audio_format;

	/**
	 * The audio_format which is really sent to the device.  This
	 * is basically config_audio_format (if configured) or
	 * in_audio_format, but may have been modified by
	 * plugin->open().
	 */
	AudioFormat out_audio_format;

	/**
	 * The filter object of this audio output.  This is an
	 * instance of chain_filter_plugin.
	 */
	PreparedFilter *prepared_filter = nullptr;

	/**
	 * The #VolumeFilter instance of this audio output.  It is
	 * used by the #SoftwareMixer.
	 */
	FilterObserver volume_filter;

	/**
	 * The replay_gain_filter_plugin instance of this audio
	 * output.
	 */
	PreparedFilter *prepared_replay_gain_filter = nullptr;

	/**
	 * The replay_gain_filter_plugin instance of this audio
	 * output, to be applied to the second chunk during
	 * cross-fading.
	 */
	PreparedFilter *prepared_other_replay_gain_filter = nullptr;

	/**
	 * The convert_filter_plugin instance of this audio output.
	 * It is the last item in the filter chain, and is responsible
	 * for converting the input data into the appropriate format
	 * for this audio output.
	 */
	FilterObserver convert_filter;

	/**
	 * This mutex protects #open, #fail_timer, #pipe.
	 */
	mutable Mutex mutex;

	/**
	 * The PlayerControl object which "owns" this output.  This
	 * object is needed to signal command completion.
	 */
	AudioOutputClient *client;

	/**
	 * Source of audio data.
	 */
	AudioOutputSource source;

	/**
	 * Throws #std::runtime_error on error.
	 */
	AudioOutput(const AudioOutputPlugin &_plugin,
		    const ConfigBlock &block);

	~AudioOutput();

private:
	void Configure(const ConfigBlock &block);

public:
	void Setup(EventLoop &event_loop,
		   const ReplayGainConfig &replay_gain_config,
		   MixerListener &mixer_listener,
		   const ConfigBlock &block);

	void BeginDestroy() noexcept;
	void FinishDestroy() noexcept;

	const char *GetName() const {
		return name;
	}

	/**
	 * Caller must lock the mutex.
	 */
	bool IsOpen() const {
		return open;
	}

	void SetReplayGainMode(ReplayGainMode _mode) noexcept {
		source.SetReplayGainMode(_mode);
	}

	/**
	 * Did we already consumed this chunk?
	 *
	 * Caller must lock the mutex.
	 */
	gcc_pure
	bool IsChunkConsumed(const MusicChunk &chunk) const noexcept;

	gcc_pure
	bool LockIsChunkConsumed(const MusicChunk &chunk) noexcept {
		const std::lock_guard<Mutex> protect(mutex);
		return IsChunkConsumed(chunk);
	}

	void ClearTailChunk(const MusicChunk &chunk) {
		source.ClearTailChunk(chunk);
	}

	/**
	 * Throws #std::runtime_error on error.
	 */
	void Enable();

	void Disable() noexcept;

	/**
	 * Throws #std::runtime_error on error.
	 */
	void Open(AudioFormat audio_format, const MusicPipe &pipe);

	void Close(bool drain) noexcept;

private:
	/**
	 * Invoke OutputPlugin::open() and configure the
	 * #ConvertFilter.
	 *
	 * Throws #std::runtime_error on error.
	 *
	 * Caller must not lock the mutex.
	 */
	void OpenOutputAndConvert(AudioFormat audio_format);

	/**
	 * Close the output plugin.
	 *
	 * Mutex must not be locked.
	 */
	void CloseOutput(bool drain) noexcept;

	/**
	 * Mutex must not be locked.
	 */
	void CloseFilter() noexcept;

public:
	void BeginPause() noexcept;
	bool IteratePause() noexcept;

	void EndPause() noexcept{
		pause = false;
	}
};

/**
 * Notify object used by the thread's client, i.e. we will send a
 * notify signal to this object, expecting the caller to wait on it.
 */
extern struct notify audio_output_client_notify;

/**
 * Throws #std::runtime_error on error.
 */
AudioOutput *
audio_output_new(EventLoop &event_loop,
		 const ReplayGainConfig &replay_gain_config,
		 const ConfigBlock &block,
		 MixerListener &mixer_listener,
		 AudioOutputClient &client);

void
audio_output_free(AudioOutput *ao) noexcept;

#endif
