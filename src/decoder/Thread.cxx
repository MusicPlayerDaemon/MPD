/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "config.h"
#include "Control.hxx"
#include "Bridge.hxx"
#include "DecoderPlugin.hxx"
#include "song/DetachedSong.hxx"
#include "MusicPipe.hxx"
#include "fs/Traits.hxx"
#include "fs/AllocatedPath.hxx"
#include "DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "input/Registry.hxx"
#include "DecoderList.hxx"
#include "system/Error.hxx"
#include "util/MimeType.hxx"
#include "util/UriExtract.hxx"
#include "util/UriUtil.hxx"
#include "util/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "util/ScopeExit.hxx"
#include "thread/Name.hxx"
#include "tag/ApeReplayGain.hxx"
#include "Log.hxx"

#include <stdexcept>
#include <functional>
#include <memory>

static constexpr Domain decoder_thread_domain("decoder_thread");

/**
 * Decode a stream with the given decoder plugin.
 *
 * Caller holds DecoderControl::mutex.
 */
static bool
decoder_stream_decode(const DecoderPlugin &plugin,
		      DecoderBridge &bridge,
		      InputStream &input_stream,
		      std::unique_lock<Mutex> &lock)
{
	assert(plugin.stream_decode != nullptr);
	assert(bridge.stream_tag == nullptr);
	assert(bridge.decoder_tag == nullptr);
	assert(input_stream.IsReady());
	assert(bridge.dc.state == DecoderState::START);

	FormatDebug(decoder_thread_domain, "probing plugin %s", plugin.name);

	if (bridge.dc.command == DecoderCommand::STOP)
		throw StopDecoder();

	/* rewind the stream, so each plugin gets a fresh start */
	try {
		input_stream.Rewind(lock);
	} catch (...) {
	}

	{
		const ScopeUnlock unlock(bridge.dc.mutex);

		FormatThreadName("decoder:%s", plugin.name);

		plugin.StreamDecode(bridge, input_stream);

		SetThreadName("decoder");
	}

	assert(bridge.dc.state == DecoderState::START ||
	       bridge.dc.state == DecoderState::DECODE);

	return bridge.dc.state != DecoderState::START;
}

/**
 * Decode a file with the given decoder plugin.
 *
 * Caller holds DecoderControl::mutex.
 */
static bool
decoder_file_decode(const DecoderPlugin &plugin,
		    DecoderBridge &bridge, Path path)
{
	assert(plugin.file_decode != nullptr);
	assert(bridge.stream_tag == nullptr);
	assert(bridge.decoder_tag == nullptr);
	assert(!path.IsNull());
	assert(path.IsAbsolute());
	assert(bridge.dc.state == DecoderState::START);

	FormatDebug(decoder_thread_domain, "probing plugin %s", plugin.name);

	if (bridge.dc.command == DecoderCommand::STOP)
		throw StopDecoder();

	{
		const ScopeUnlock unlock(bridge.dc.mutex);

		FormatThreadName("decoder:%s", plugin.name);

		plugin.FileDecode(bridge, path);

		SetThreadName("decoder");
	}

	assert(bridge.dc.state == DecoderState::START ||
	       bridge.dc.state == DecoderState::DECODE);

	return bridge.dc.state != DecoderState::START;
}

gcc_pure
static bool
decoder_check_plugin_mime(const DecoderPlugin &plugin,
			  const InputStream &is) noexcept
{
	assert(plugin.stream_decode != nullptr);

	const char *mime_type = is.GetMimeType();
	return mime_type != nullptr &&
		plugin.SupportsMimeType(GetMimeTypeBase(mime_type).c_str());
}

gcc_pure
static bool
decoder_check_plugin_suffix(const DecoderPlugin &plugin,
			    const char *suffix) noexcept
{
	assert(plugin.stream_decode != nullptr);

	return suffix != nullptr && plugin.SupportsSuffix(suffix);
}

gcc_pure
static bool
decoder_check_plugin(const DecoderPlugin &plugin, const InputStream &is,
		     const char *suffix) noexcept
{
	return plugin.stream_decode != nullptr &&
		(decoder_check_plugin_mime(plugin, is) ||
		 decoder_check_plugin_suffix(plugin, suffix));
}

static bool
decoder_run_stream_plugin(DecoderBridge &bridge, InputStream &is,
			  std::unique_lock<Mutex> &lock,
			  const char *suffix,
			  const DecoderPlugin &plugin,
			  bool &tried_r)
{
	if (!decoder_check_plugin(plugin, is, suffix))
		return false;

	bridge.Reset();

	tried_r = true;
	return decoder_stream_decode(plugin, bridge, is, lock);
}

static bool
decoder_run_stream_locked(DecoderBridge &bridge, InputStream &is,
			  std::unique_lock<Mutex> &lock,
			  const char *uri, bool &tried_r)
{
	UriSuffixBuffer suffix_buffer;
	const char *const suffix = uri_get_suffix(uri, suffix_buffer);

	using namespace std::placeholders;
	const auto f = std::bind(decoder_run_stream_plugin,
				 std::ref(bridge), std::ref(is), std::ref(lock),
				 suffix,
				 _1, std::ref(tried_r));
	return decoder_plugins_try(f);
}

/**
 * Try decoding a stream, using the fallback plugin.
 */
static bool
decoder_run_stream_fallback(DecoderBridge &bridge, InputStream &is,
			    std::unique_lock<Mutex> &lock)
{
	const struct DecoderPlugin *plugin;

#ifdef ENABLE_FFMPEG
	plugin = decoder_plugin_from_name("ffmpeg");
#else
	plugin = decoder_plugin_from_name("mad");
#endif
	return plugin != nullptr && plugin->stream_decode != nullptr &&
		decoder_stream_decode(*plugin, bridge, is, lock);
}

/**
 * Attempt to load replay gain data, and pass it to
 * DecoderClient::SubmitReplayGain().
 */
static void
LoadReplayGain(DecoderClient &client, InputStream &is)
{
	ReplayGainInfo info;
	if (replay_gain_ape_read(is, info))
		client.SubmitReplayGain(&info);
}

/**
 * Call LoadReplayGain() unless ReplayGain is disabled.  This saves
 * the I/O overhead when the user is not interested in the feature.
 */
static void
MaybeLoadReplayGain(DecoderBridge &bridge, InputStream &is)
{
	{
		const std::lock_guard<Mutex> protect(bridge.dc.mutex);
		if (bridge.dc.replay_gain_mode == ReplayGainMode::OFF)
			/* ReplayGain is disabled */
			return;
	}

	LoadReplayGain(bridge, is);
}

/**
 * Try decoding a stream.
 *
 * DecoderControl::mutex is not locked by caller.
 */
static bool
decoder_run_stream(DecoderBridge &bridge, const char *uri)
{
	DecoderControl &dc = bridge.dc;

	auto input_stream = bridge.OpenUri(uri);
	assert(input_stream);

	MaybeLoadReplayGain(bridge, *input_stream);

	std::unique_lock<Mutex> lock(dc.mutex);

	bool tried = false;
	return dc.command == DecoderCommand::STOP ||
		decoder_run_stream_locked(bridge, *input_stream, lock, uri,
					  tried) ||
		/* fallback to mp3: this is needed for bastard streams
		   that don't have a suffix or set the mimeType */
		(!tried &&
		 decoder_run_stream_fallback(bridge, *input_stream, lock));
}

/**
 * Decode a file with the given decoder plugin.
 *
 * DecoderControl::mutex is not locked by caller.
 */
static bool
TryDecoderFile(DecoderBridge &bridge, Path path_fs, const char *suffix,
	       InputStream &input_stream,
	       const DecoderPlugin &plugin)
{
	if (!plugin.SupportsSuffix(suffix))
		return false;

	bridge.Reset();

	DecoderControl &dc = bridge.dc;

	if (plugin.file_decode != nullptr) {
		const std::lock_guard<Mutex> protect(dc.mutex);
		return decoder_file_decode(plugin, bridge, path_fs);
	} else if (plugin.stream_decode != nullptr) {
		std::unique_lock<Mutex> lock(dc.mutex);
		return decoder_stream_decode(plugin, bridge, input_stream,
					     lock);
	} else
		return false;
}

/**
 * Decode a container file with the given decoder plugin.
 *
 * DecoderControl::mutex is not locked by caller.
 */
static bool
TryContainerDecoder(DecoderBridge &bridge, Path path_fs, const char *suffix,
		    const DecoderPlugin &plugin)
{
	if (plugin.container_scan == nullptr ||
	    plugin.file_decode == nullptr ||
	    !plugin.SupportsSuffix(suffix))
		return false;

	bridge.Reset();

	DecoderControl &dc = bridge.dc;
	const std::lock_guard<Mutex> protect(dc.mutex);
	return decoder_file_decode(plugin, bridge, path_fs);
}

/**
 * Decode a container file.
 *
 * DecoderControl::mutex is not locked by caller.
 */
static bool
TryContainerDecoder(DecoderBridge &bridge, Path path_fs, const char *suffix)
{
	return decoder_plugins_try([&bridge, path_fs,
				    suffix](const DecoderPlugin &plugin){
					   return TryContainerDecoder(bridge,
								      path_fs,
								      suffix,
								      plugin);
				   });
}

/**
 * Try decoding a file.
 *
 * DecoderControl::mutex is not locked by caller.
 */
static bool
decoder_run_file(DecoderBridge &bridge, const char *uri_utf8, Path path_fs)
{
	const char *suffix = uri_get_suffix(uri_utf8);
	if (suffix == nullptr)
		return false;

	InputStreamPtr input_stream;

	try {
		input_stream = bridge.OpenLocal(path_fs, uri_utf8);
	} catch (const std::system_error &e) {
		if (IsPathNotFound(e) &&
		    /* ENOTDIR means this may be a path inside a
		       "container" file */
		    TryContainerDecoder(bridge, path_fs, suffix))
			return true;

		throw;
	}

	assert(input_stream);

	MaybeLoadReplayGain(bridge, *input_stream);

	auto &is = *input_stream;
	return decoder_plugins_try([&bridge, path_fs, suffix,
				    &is](const DecoderPlugin &plugin){
					   return TryDecoderFile(bridge,
								 path_fs,
								 suffix,
								 is,
								 plugin);
				   });
}

/**
 * Decode a song.
 *
 * DecoderControl::mutex is not locked.
 */
static bool
DecoderUnlockedRunUri(DecoderBridge &bridge,
		      const char *real_uri, Path path_fs)
try {
	return !path_fs.IsNull()
		? decoder_run_file(bridge, real_uri, path_fs)
		: decoder_run_stream(bridge, real_uri);
} catch (StopDecoder) {
	return true;
} catch (...) {
	const char *error_uri = real_uri;
	const std::string allocated = uri_remove_auth(error_uri);
	if (!allocated.empty())
		error_uri = allocated.c_str();

	std::throw_with_nested(FormatRuntimeError("Failed to decode %s",
						  error_uri));
}

/**
 * Try to guess whether tags attached to the given song are
 * "volatile", e.g. if they have been received by a live stream, but
 * are only kept as a cache to be displayed by the client; they shall
 * not be sent to the output.
 */
gcc_pure
static bool
SongHasVolatileTags(const DetachedSong &song) noexcept
{
	return !song.IsFile() && !HasRemoteTagScanner(song.GetRealURI());
}

/**
 * Decode a song addressed by a #DetachedSong.
 *
 * Caller holds DecoderControl::mutex.
 */
static void
decoder_run_song(DecoderControl &dc,
		 const DetachedSong &song, const char *uri, Path path_fs)
{
	if (dc.command == DecoderCommand::SEEK)
		/* if the SEEK command arrived too late, start the
		   decoder at the seek position */
		dc.start_time = dc.seek_time;

	DecoderBridge bridge(dc, dc.start_time.IsPositive(),
			     /* pass the song tag only if it's
				authoritative, i.e. if it's a local
				file - tags on "stream" songs are just
				remembered from the last time we
				played it*/
			     !SongHasVolatileTags(song) ? std::make_unique<Tag>(song.GetTag()) : nullptr);

	dc.state = DecoderState::START;
	dc.CommandFinishedLocked();

	bool success;
	{
		const ScopeUnlock unlock(dc.mutex);

		AtScopeExit(&bridge) {
			/* flush the last chunk */
			bridge.CheckFlushChunk();
		};

		success = DecoderUnlockedRunUri(bridge, uri, path_fs);

	}

	bridge.CheckRethrowError();

	if (success)
		dc.state = DecoderState::STOP;
	else {
		const char *error_uri = song.GetURI();
		const std::string allocated = uri_remove_auth(error_uri);
		if (!allocated.empty())
			error_uri = allocated.c_str();

		throw FormatRuntimeError("Failed to decode %s", error_uri);
	}

	dc.client_cond.notify_one();
}

/**
 *
 * Caller holds DecoderControl::mutex.
 */
static void
decoder_run(DecoderControl &dc) noexcept
try {
	dc.ClearError();

	assert(dc.song != nullptr);
	const DetachedSong &song = *dc.song;

	const char *const uri_utf8 = song.GetRealURI();

	Path path_fs = nullptr;
	AllocatedPath path_buffer = nullptr;
	if (PathTraitsUTF8::IsAbsolute(uri_utf8)) {
		path_buffer = AllocatedPath::FromUTF8Throw(uri_utf8);
		path_fs = path_buffer;
	}

	decoder_run_song(dc, song, uri_utf8, path_fs);
} catch (...) {
	dc.state = DecoderState::ERROR;
	dc.command = DecoderCommand::NONE;
	dc.error = std::current_exception();
	dc.client_cond.notify_one();
}

void
DecoderControl::RunThread() noexcept
{
	SetThreadName("decoder");

	std::unique_lock<Mutex> lock(mutex);

	do {
		assert(state == DecoderState::STOP ||
		       state == DecoderState::ERROR);

		switch (command) {
		case DecoderCommand::START:
			CycleMixRamp();
			replay_gain_prev_db = replay_gain_db;
			replay_gain_db = 0;

			decoder_run(*this);

			if (state == DecoderState::ERROR) {
				try {
					std::rethrow_exception(error);
				} catch (...) {
					LogError(std::current_exception());
				}
			}

			break;

		case DecoderCommand::SEEK:
			/* this seek was too late, and the decoder had
			   already finished; start a new decoder */

			/* we need to clear the pipe here; usually the
			   PlayerThread is responsible, but it is not
			   aware that the decoder has finished */
			pipe->Clear();

			decoder_run(*this);
			break;

		case DecoderCommand::STOP:
			CommandFinishedLocked();
			break;

		case DecoderCommand::NONE:
			Wait(lock);
			break;
		}
	} while (command != DecoderCommand::NONE || !quit);
}
