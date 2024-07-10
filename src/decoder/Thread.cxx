// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
#include "lib/fmt/RuntimeError.hxx"
#include "system/Error.hxx"
#include "util/MimeType.hxx"
#include "util/UriExtract.hxx"
#include "util/UriUtil.hxx"
#include "util/Domain.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringCompare.hxx"
#include "thread/Name.hxx"
#include "tag/ApeReplayGain.hxx"
#include "Log.hxx"

#include <stdexcept>
#include <functional>
#include <memory>

static constexpr Domain decoder_thread_domain("decoder_thread");

enum class DecodeResult {
	/**
	 * No plugin supporting this file type was found.
	 */
	NO_PLUGIN,

	/**
	 * A plugin was found, but it does not support streaming.
	 */
	NO_STREAM_PLUGIN,

	/**
	 * A plugin was found, but it did not recgonize the file.
	 */
	UNRECOGNIZED_FILE,

	/**
	 * A "stop" command was found before decoder initialization
	 * was completed.
	 */
	STOP,

	/**
	 * The file was decoded successfully.
	 */
	SUCCESS,
};

static constexpr bool
IsFinalDecodeResult(DecodeResult result) noexcept
{
	return result >= DecodeResult::STOP;
}

/**
 * Decode a URI with the given decoder plugin.
 *
 * Caller holds DecoderControl::mutex.
 */
static DecodeResult
DecoderUriDecode(const DecoderPlugin &plugin,
		 DecoderBridge &bridge, const char *uri)
{
	assert(plugin.uri_decode != nullptr);
	assert(bridge.stream_tag == nullptr);
	assert(bridge.decoder_tag == nullptr);
	assert(uri != nullptr);
	assert(bridge.dc.state == DecoderState::START);

	FmtDebug(decoder_thread_domain, "probing plugin {}", plugin.name);

	if (bridge.dc.command == DecoderCommand::STOP)
		return DecodeResult::STOP;

	{
		const ScopeUnlock unlock(bridge.dc.mutex);

		FmtThreadName("decoder:{}", plugin.name);

		plugin.UriDecode(bridge, uri);

		SetThreadName("decoder");
	}

	assert(bridge.dc.state == DecoderState::START ||
	       bridge.dc.state == DecoderState::DECODE);

	return bridge.dc.state == DecoderState::START
	       ? DecodeResult::UNRECOGNIZED_FILE
	       : DecodeResult::SUCCESS;
}

/**
 * Decode a stream with the given decoder plugin.
 *
 * Caller holds DecoderControl::mutex.
 */
static DecodeResult
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

	FmtDebug(decoder_thread_domain, "probing plugin {}", plugin.name);

	if (bridge.dc.command == DecoderCommand::STOP)
		return DecodeResult::STOP;

	/* rewind the stream, so each plugin gets a fresh start */
	try {
		input_stream.Rewind(lock);
	} catch (...) {
	}

	{
		const ScopeUnlock unlock(bridge.dc.mutex);

		FmtThreadName("decoder:{}", plugin.name);

		plugin.StreamDecode(bridge, input_stream);

		SetThreadName("decoder");
	}

	assert(bridge.dc.state == DecoderState::START ||
	       bridge.dc.state == DecoderState::DECODE);

	return bridge.dc.state == DecoderState::START
	       ? DecodeResult::UNRECOGNIZED_FILE
	       : DecodeResult::SUCCESS;
}

/**
 * Decode a file with the given decoder plugin.
 *
 * Caller holds DecoderControl::mutex.
 */
static DecodeResult
decoder_file_decode(const DecoderPlugin &plugin,
		    DecoderBridge &bridge, Path path)
{
	assert(plugin.file_decode != nullptr);
	assert(bridge.stream_tag == nullptr);
	assert(bridge.decoder_tag == nullptr);
	assert(!path.IsNull());
	assert(path.IsAbsolute());
	assert(bridge.dc.state == DecoderState::START);

	FmtDebug(decoder_thread_domain, "probing plugin {}", plugin.name);

	if (bridge.dc.command == DecoderCommand::STOP)
		return DecodeResult::STOP;

	{
		const ScopeUnlock unlock(bridge.dc.mutex);

		FmtThreadName("decoder:{}", plugin.name);

		plugin.FileDecode(bridge, path);

		SetThreadName("decoder");
	}

	assert(bridge.dc.state == DecoderState::START ||
	       bridge.dc.state == DecoderState::DECODE);

	return bridge.dc.state == DecoderState::START
	       ? DecodeResult::UNRECOGNIZED_FILE
	       : DecodeResult::SUCCESS;
}

[[gnu::pure]]
static bool
decoder_check_plugin_mime(const DecoderPlugin &plugin,
			  const InputStream &is) noexcept
{
	const char *mime_type = is.GetMimeType();
	return mime_type != nullptr &&
		plugin.SupportsMimeType(GetMimeTypeBase(mime_type));
}

[[gnu::pure]]
static bool
decoder_check_plugin_suffix(const DecoderPlugin &plugin,
			    std::string_view suffix) noexcept
{
	return !suffix.empty() && plugin.SupportsSuffix(suffix);
}

static DecodeResult
decoder_run_stream_plugin(DecoderBridge &bridge, InputStream &is,
			  std::unique_lock<Mutex> &lock,
			  std::string_view suffix,
			  const DecoderPlugin &plugin)
{
	if (!decoder_check_plugin_mime(plugin, is) &&
	    !decoder_check_plugin_suffix(plugin, suffix))
		return DecodeResult::NO_PLUGIN;

	if (plugin.stream_decode == nullptr)
		return DecodeResult::NO_STREAM_PLUGIN;

	bridge.Reset();

	return decoder_stream_decode(plugin, bridge, is, lock);
}

static DecodeResult
decoder_run_stream_locked(DecoderBridge &bridge, InputStream &is,
			  std::unique_lock<Mutex> &lock,
			  const char *uri)
{
	const auto suffix = uri_get_suffix(uri);

	DecodeResult result = DecodeResult::NO_PLUGIN;
	for (const auto &plugin : GetEnabledDecoderPlugins()) {
		const auto r = decoder_run_stream_plugin(bridge, is, lock, suffix, plugin);
		if (r > result) {
			result = r;
			if (IsFinalDecodeResult(result))
				break;
		}
	}

	return result;
}

/**
 * Try decoding a stream, using the fallback plugin.
 */
static DecodeResult
decoder_run_stream_fallback(DecoderBridge &bridge, InputStream &is,
			    std::unique_lock<Mutex> &lock)
{
	const struct DecoderPlugin *plugin;

#ifdef ENABLE_FFMPEG
	plugin = decoder_plugin_from_name("ffmpeg");
#else
	plugin = decoder_plugin_from_name("mad");
#endif
	if (plugin == nullptr)
		return DecodeResult::NO_PLUGIN;

	if (plugin->stream_decode == nullptr)
		return DecodeResult::NO_STREAM_PLUGIN;

	return decoder_stream_decode(*plugin, bridge, is, lock);
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
	if (!bridge.dc.LockIsReplayGainEnabled())
		/* ReplayGain is disabled */
		return;

	if (is.HasMimeType() &&
	    StringStartsWith(is.GetMimeType(), "audio/x-mpd-"))
		/* skip for (virtual) files (e.g. from the
		   cdio_paranoia input plugin) which cannot possibly
		   contain tags */
		return;

	LoadReplayGain(bridge, is);
}

/**
 * Try decoding a URI.
 *
 * DecoderControl::mutex is not be locked by caller.
 */
static DecodeResult
TryUriDecode(DecoderBridge &bridge, const char *uri)
{
	DecodeResult result = DecodeResult::NO_PLUGIN;

	for (const auto &plugin : GetEnabledDecoderPlugins()) {
		if (!plugin.SupportsUri(uri))
			continue;

		std::unique_lock lock{bridge.dc.mutex};
		bridge.Reset();

		if (const auto r = DecoderUriDecode(plugin, bridge, uri);
		    r > result) {
			result = r;
			if (IsFinalDecodeResult(result))
				break;
		}
	}

	return result;
}

/**
 * Try decoding a stream.
 *
 * DecoderControl::mutex is not locked by caller.
 */
static DecodeResult
decoder_run_stream(DecoderBridge &bridge, const char *uri)
{
	auto result = TryUriDecode(bridge, uri);
	if (IsFinalDecodeResult(result))
		return result;

	DecoderControl &dc = bridge.dc;

	auto input_stream = bridge.OpenUri(uri);
	assert(input_stream);

	MaybeLoadReplayGain(bridge, *input_stream);

	std::unique_lock lock{dc.mutex};

	if (dc.command == DecoderCommand::STOP)
		return DecodeResult::STOP;

	if (auto r = decoder_run_stream_locked(bridge, *input_stream, lock, uri);
	    r > result) {
		result = r;
		if (IsFinalDecodeResult(result))
			return result;
	}

	/* fallback to mp3: this is needed for bastard streams that
	   don't have a suffix or set the mimeType */
	if (auto r = decoder_run_stream_fallback(bridge, *input_stream, lock);
	    r > result) {
		result = r;
	}

	return result;
}

/**
 * Decode a file with the given decoder plugin.
 *
 * DecoderControl::mutex is not locked by caller.
 */
static DecodeResult
TryDecoderFile(DecoderBridge &bridge, Path path_fs, std::string_view suffix,
	       InputStream &input_stream,
	       const DecoderPlugin &plugin)
{
	if (!plugin.SupportsSuffix(suffix))
		return DecodeResult::NO_PLUGIN;

	bridge.Reset();

	DecoderControl &dc = bridge.dc;

	if (plugin.file_decode != nullptr) {
		const std::scoped_lock protect{dc.mutex};
		return decoder_file_decode(plugin, bridge, path_fs);
	} else if (plugin.stream_decode != nullptr) {
		std::unique_lock lock{dc.mutex};
		return decoder_stream_decode(plugin, bridge, input_stream,
					     lock);
	} else
		return DecodeResult::NO_STREAM_PLUGIN;
}

/**
 * Decode a container file with the given decoder plugin.
 *
 * DecoderControl::mutex is not locked by caller.
 */
static DecodeResult
TryContainerDecoder(DecoderBridge &bridge, Path path_fs,
		    std::string_view suffix,
		    const DecoderPlugin &plugin)
{
	if (plugin.container_scan == nullptr ||
	    plugin.file_decode == nullptr ||
	    !plugin.SupportsSuffix(suffix))
		return DecodeResult::NO_PLUGIN;

	bridge.Reset();

	DecoderControl &dc = bridge.dc;
	const std::scoped_lock protect{dc.mutex};
	return decoder_file_decode(plugin, bridge, path_fs);
}

/**
 * Decode a container file.
 *
 * DecoderControl::mutex is not locked by caller.
 */
static DecodeResult
TryContainerDecoder(DecoderBridge &bridge, Path path_fs,
		    std::string_view suffix)
{
	DecodeResult result = DecodeResult::NO_PLUGIN;

	for (const auto &plugin : GetEnabledDecoderPlugins()) {
		if (const auto r = TryContainerDecoder(bridge, path_fs, suffix, plugin);
		    r > result) {
			result = r;
			if (IsFinalDecodeResult(result))
				break;
		}
	}

	return result;
}

/**
 * Try decoding a file.
 *
 * DecoderControl::mutex is not locked by caller.
 */
static DecodeResult
decoder_run_file(DecoderBridge &bridge, const char *uri_utf8, Path path_fs)
{
	const char *_suffix = PathTraitsUTF8::GetFilenameSuffix(uri_utf8);
	if (_suffix == nullptr)
		return DecodeResult::NO_PLUGIN;

	const std::string_view suffix{_suffix};

	InputStreamPtr input_stream;

	try {
		input_stream = bridge.OpenLocal(path_fs, uri_utf8);
	} catch (const std::system_error &e) {
		if (IsPathNotFound(e)) {
		    /* ENOTDIR means this may be a path inside a
		       "container" file */
			const auto result = TryContainerDecoder(bridge, path_fs, suffix);
			if (IsFinalDecodeResult(result))
				return result;
		}

		throw;
	}

	assert(input_stream);

	MaybeLoadReplayGain(bridge, *input_stream);

	DecodeResult result = DecodeResult::NO_PLUGIN;
	for (const auto &plugin : GetEnabledDecoderPlugins()) {
		if (const auto r = TryDecoderFile(bridge, path_fs, suffix, *input_stream, plugin);
		    r > result) {
			result = r;
			if (IsFinalDecodeResult(result))
				break;
		}
	}

	return result;
}

/**
 * Decode a song.
 *
 * DecoderControl::mutex is not locked.
 */
static DecodeResult
DecoderUnlockedRunUri(DecoderBridge &bridge,
		      const char *real_uri, Path path_fs)
try {
	return !path_fs.IsNull()
		? decoder_run_file(bridge, real_uri, path_fs)
		: decoder_run_stream(bridge, real_uri);
} catch (StopDecoder) {
	return DecodeResult::STOP;
} catch (...) {
	const char *error_uri = real_uri;
	const std::string allocated = uri_remove_auth(error_uri);
	if (!allocated.empty())
		error_uri = allocated.c_str();

	std::throw_with_nested(FmtRuntimeError("Failed to decode {:?}",
					       error_uri));
}

/**
 * Try to guess whether tags attached to the given song are
 * "volatile", e.g. if they have been received by a live stream, but
 * are only kept as a cache to be displayed by the client; they shall
 * not be sent to the output.
 */
[[gnu::pure]]
static bool
SongHasVolatileTags(const DetachedSong &song) noexcept
{
	return !song.IsFile() && !HasRemoteTagScanner(song.GetRealURI());
}

[[gnu::pure]]
static std::runtime_error
MakeDecoderError(const DetachedSong &song, const char *msg) noexcept
{
	const char *error_uri = song.GetURI();
	const std::string allocated = uri_remove_auth(error_uri);
	if (!allocated.empty())
		error_uri = allocated.c_str();

	return FmtRuntimeError("Failed to decode {:?}: {}", error_uri, msg);
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
			     dc.initial_seek_essential,
			     /* pass the song tag only if it's
				authoritative, i.e. if it's a local
				file - tags on "stream" songs are just
				remembered from the last time we
				played it*/
			     !SongHasVolatileTags(song) ? std::make_unique<Tag>(song.GetTag()) : nullptr);

	dc.state = DecoderState::START;
	dc.CommandFinishedLocked();

	DecodeResult result;
	{
		const ScopeUnlock unlock(dc.mutex);

		AtScopeExit(&bridge) {
			/* flush the last chunk */
			bridge.CheckFlushChunk();
		};

		result = DecoderUnlockedRunUri(bridge, uri, path_fs);
	}

	bridge.CheckRethrowError();

	switch (result) {
	case DecodeResult::NO_PLUGIN:
		throw MakeDecoderError(song, "no decoder plugin");

	case DecodeResult::NO_STREAM_PLUGIN:
		throw MakeDecoderError(song, "no streaming decoder plugin");

	case DecodeResult::UNRECOGNIZED_FILE:
		throw MakeDecoderError(song, "unrecognized file");

	case DecodeResult::STOP:
	case DecodeResult::SUCCESS:
		dc.state = DecoderState::STOP;
		break;
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

	std::unique_lock lock{mutex};

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
