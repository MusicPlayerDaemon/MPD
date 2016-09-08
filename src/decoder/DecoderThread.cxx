/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "DecoderThread.hxx"
#include "DecoderControl.hxx"
#include "DecoderInternal.hxx"
#include "DecoderError.hxx"
#include "DecoderPlugin.hxx"
#include "DetachedSong.hxx"
#include "MusicPipe.hxx"
#include "fs/Traits.hxx"
#include "fs/AllocatedPath.hxx"
#include "DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"
#include "DecoderList.hxx"
#include "util/MimeType.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
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
 * Opens the input stream with InputStream::Open(), and waits until
 * the stream gets ready.
 *
 * Unlock the decoder before calling this function.
 */
static InputStreamPtr
decoder_input_stream_open(DecoderControl &dc, const char *uri)
{
	Error error;
	auto is = InputStream::Open(uri, dc.mutex, dc.cond, error);
	if (is == nullptr)
		throw error;

	/* wait for the input stream to become ready; its metadata
	   will be available then */

	const ScopeLock protect(dc.mutex);

	is->Update();
	while (!is->IsReady()) {
		if (dc.command == DecoderCommand::STOP)
			throw StopDecoder();

		dc.Wait();

		is->Update();
	}

	if (!is->Check(error))
		throw error;

	return is;
}

static InputStreamPtr
decoder_input_stream_open(DecoderControl &dc, Path path)
{
	Error error;
	auto is = OpenLocalInputStream(path, dc.mutex, dc.cond, error);
	if (is == nullptr)
		throw error;

	assert(is->IsReady());

	return is;
}

/**
 * Decode a stream with the given decoder plugin.
 *
 * Caller holds DecoderControl::mutex.
 */
static bool
decoder_stream_decode(const DecoderPlugin &plugin,
		      Decoder &decoder,
		      InputStream &input_stream)
{
	assert(plugin.stream_decode != nullptr);
	assert(decoder.stream_tag == nullptr);
	assert(decoder.decoder_tag == nullptr);
	assert(input_stream.IsReady());
	assert(decoder.dc.state == DecoderState::START);

	FormatDebug(decoder_thread_domain, "probing plugin %s", plugin.name);

	if (decoder.dc.command == DecoderCommand::STOP)
		throw StopDecoder();

	/* rewind the stream, so each plugin gets a fresh start */
	input_stream.Rewind(IgnoreError());

	{
		const ScopeUnlock unlock(decoder.dc.mutex);

		FormatThreadName("decoder:%s", plugin.name);

		plugin.StreamDecode(decoder, input_stream);

		SetThreadName("decoder");
	}

	assert(decoder.dc.state == DecoderState::START ||
	       decoder.dc.state == DecoderState::DECODE);

	return decoder.dc.state != DecoderState::START;
}

/**
 * Decode a file with the given decoder plugin.
 *
 * Caller holds DecoderControl::mutex.
 */
static bool
decoder_file_decode(const DecoderPlugin &plugin,
		    Decoder &decoder, Path path)
{
	assert(plugin.file_decode != nullptr);
	assert(decoder.stream_tag == nullptr);
	assert(decoder.decoder_tag == nullptr);
	assert(!path.IsNull());
	assert(path.IsAbsolute());
	assert(decoder.dc.state == DecoderState::START);

	FormatDebug(decoder_thread_domain, "probing plugin %s", plugin.name);

	if (decoder.dc.command == DecoderCommand::STOP)
		throw StopDecoder();

	{
		const ScopeUnlock unlock(decoder.dc.mutex);

		FormatThreadName("decoder:%s", plugin.name);

		plugin.FileDecode(decoder, path);

		SetThreadName("decoder");
	}

	assert(decoder.dc.state == DecoderState::START ||
	       decoder.dc.state == DecoderState::DECODE);

	return decoder.dc.state != DecoderState::START;
}

gcc_pure
static bool
decoder_check_plugin_mime(const DecoderPlugin &plugin, const InputStream &is)
{
	assert(plugin.stream_decode != nullptr);

	const char *mime_type = is.GetMimeType();
	return mime_type != nullptr &&
		plugin.SupportsMimeType(GetMimeTypeBase(mime_type).c_str());
}

gcc_pure
static bool
decoder_check_plugin_suffix(const DecoderPlugin &plugin, const char *suffix)
{
	assert(plugin.stream_decode != nullptr);

	return suffix != nullptr && plugin.SupportsSuffix(suffix);
}

gcc_pure
static bool
decoder_check_plugin(const DecoderPlugin &plugin, const InputStream &is,
		     const char *suffix)
{
	return plugin.stream_decode != nullptr &&
		(decoder_check_plugin_mime(plugin, is) ||
		 decoder_check_plugin_suffix(plugin, suffix));
}

static bool
decoder_run_stream_plugin(Decoder &decoder, InputStream &is,
			  const char *suffix,
			  const DecoderPlugin &plugin,
			  bool &tried_r)
{
	if (!decoder_check_plugin(plugin, is, suffix))
		return false;

	decoder.error.Clear();

	tried_r = true;
	return decoder_stream_decode(plugin, decoder, is);
}

static bool
decoder_run_stream_locked(Decoder &decoder, InputStream &is,
			  const char *uri, bool &tried_r)
{
	UriSuffixBuffer suffix_buffer;
	const char *const suffix = uri_get_suffix(uri, suffix_buffer);

	using namespace std::placeholders;
	const auto f = std::bind(decoder_run_stream_plugin,
				 std::ref(decoder), std::ref(is), suffix,
				 _1, std::ref(tried_r));
	return decoder_plugins_try(f);
}

/**
 * Try decoding a stream, using the fallback plugin.
 */
static bool
decoder_run_stream_fallback(Decoder &decoder, InputStream &is)
{
	const struct DecoderPlugin *plugin;

#ifdef HAVE_FFMPEG
	plugin = decoder_plugin_from_name("ffmpeg");
#else
	plugin = decoder_plugin_from_name("mad");
#endif
	return plugin != nullptr && plugin->stream_decode != nullptr &&
		decoder_stream_decode(*plugin, decoder, is);
}

/**
 * Attempt to load replay gain data, and pass it to
 * decoder_replay_gain().
 */
static void
LoadReplayGain(Decoder &decoder, InputStream &is)
{
	ReplayGainInfo info;
	if (replay_gain_ape_read(is, info))
		decoder_replay_gain(decoder, &info);
}

/**
 * Try decoding a stream.
 *
 * DecoderControl::mutex is not locked by caller.
 */
static bool
decoder_run_stream(Decoder &decoder, const char *uri)
{
	DecoderControl &dc = decoder.dc;

	auto input_stream = decoder_input_stream_open(dc, uri);
	assert(input_stream);

	LoadReplayGain(decoder, *input_stream);

	const ScopeLock protect(dc.mutex);

	bool tried = false;
	return dc.command == DecoderCommand::STOP ||
		decoder_run_stream_locked(decoder, *input_stream, uri,
					  tried) ||
		/* fallback to mp3: this is needed for bastard streams
		   that don't have a suffix or set the mimeType */
		(!tried &&
		 decoder_run_stream_fallback(decoder, *input_stream));
}

/**
 * Decode a file with the given decoder plugin.
 *
 * DecoderControl::mutex is not locked by caller.
 */
static bool
TryDecoderFile(Decoder &decoder, Path path_fs, const char *suffix,
	       InputStream &input_stream,
	       const DecoderPlugin &plugin)
{
	if (!plugin.SupportsSuffix(suffix))
		return false;

	decoder.error.Clear();

	DecoderControl &dc = decoder.dc;

	if (plugin.file_decode != nullptr) {
		const ScopeLock protect(dc.mutex);
		return decoder_file_decode(plugin, decoder, path_fs);
	} else if (plugin.stream_decode != nullptr) {
		const ScopeLock protect(dc.mutex);
		return decoder_stream_decode(plugin, decoder, input_stream);
	} else
		return false;
}

/**
 * Try decoding a file.
 *
 * DecoderControl::mutex is not locked by caller.
 */
static bool
decoder_run_file(Decoder &decoder, const char *uri_utf8, Path path_fs)
{
	const char *suffix = uri_get_suffix(uri_utf8);
	if (suffix == nullptr)
		return false;

	auto input_stream = decoder_input_stream_open(decoder.dc, path_fs);
	assert(input_stream);

	LoadReplayGain(decoder, *input_stream);

	auto &is = *input_stream;
	return decoder_plugins_try([&decoder, path_fs, suffix,
				    &is](const DecoderPlugin &plugin){
					   return TryDecoderFile(decoder,
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
DecoderUnlockedRunUri(Decoder &decoder, const char *real_uri, Path path_fs)
try {
	return !path_fs.IsNull()
		? decoder_run_file(decoder, real_uri, path_fs)
		: decoder_run_stream(decoder, real_uri);
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
 * Decode a song addressed by a #DetachedSong.
 *
 * Caller holds DecoderControl::mutex.
 */
static void
decoder_run_song(DecoderControl &dc,
		 const DetachedSong &song, const char *uri, Path path_fs)
{
	Decoder decoder(dc, dc.start_time.IsPositive(),
			/* pass the song tag only if it's
			   authoritative, i.e. if it's a local file -
			   tags on "stream" songs are just remembered
			   from the last time we played it*/
			song.IsFile() ? new Tag(song.GetTag()) : nullptr);

	dc.state = DecoderState::START;
	dc.CommandFinishedLocked();

	bool success;
	{
		const ScopeUnlock unlock(dc.mutex);

		AtScopeExit(&decoder) {
			/* flush the last chunk */
			if (decoder.chunk != nullptr)
				decoder.FlushChunk();
		};

		success = DecoderUnlockedRunUri(decoder, uri, path_fs);

	}

	if (decoder.error.IsDefined()) {
		/* copy the Error from struct Decoder to
		   DecoderControl */
		throw std::move(decoder.error);
	} else if (success)
		dc.state = DecoderState::STOP;
	else {
		const char *error_uri = song.GetURI();
		const std::string allocated = uri_remove_auth(error_uri);
		if (!allocated.empty())
			error_uri = allocated.c_str();

		throw FormatRuntimeError("Failed to decode %s", error_uri);
	}

	dc.client_cond.signal();
}

/**
 *
 * Caller holds DecoderControl::mutex.
 */
static void
decoder_run(DecoderControl &dc)
try {
	dc.ClearError();

	assert(dc.song != nullptr);
	const DetachedSong &song = *dc.song;

	const char *const uri_utf8 = song.GetRealURI();

	Path path_fs = Path::Null();
	AllocatedPath path_buffer = AllocatedPath::Null();
	if (PathTraitsUTF8::IsAbsolute(uri_utf8)) {
		Error error;
		path_buffer = AllocatedPath::FromUTF8(uri_utf8, error);
		if (path_buffer.IsNull()) {
			dc.CommandFinishedLocked();
			throw std::move(error);
		}

		path_fs = path_buffer;
	}

	decoder_run_song(dc, song, uri_utf8, path_fs);
} catch (...) {
	dc.state = DecoderState::ERROR;
	dc.error = std::current_exception();
	dc.client_cond.signal();
}

static void
decoder_task(void *arg)
{
	DecoderControl &dc = *(DecoderControl *)arg;

	SetThreadName("decoder");

	const ScopeLock protect(dc.mutex);

	do {
		assert(dc.state == DecoderState::STOP ||
		       dc.state == DecoderState::ERROR);

		switch (dc.command) {
		case DecoderCommand::START:
			dc.CycleMixRamp();
			dc.replay_gain_prev_db = dc.replay_gain_db;
			dc.replay_gain_db = 0;

			decoder_run(dc);

			if (dc.state == DecoderState::ERROR) {
				try {
					std::rethrow_exception(dc.error);
				} catch (const std::exception &e) {
					LogError(e);
				} catch (const Error &error) {
					LogError(error);
				} catch (...) {
				}
			}

			break;

		case DecoderCommand::SEEK:
			/* this seek was too late, and the decoder had
			   already finished; start a new decoder */

			/* we need to clear the pipe here; usually the
			   PlayerThread is responsible, but it is not
			   aware that the decoder has finished */
			dc.pipe->Clear(*dc.buffer);

			decoder_run(dc);
			break;

		case DecoderCommand::STOP:
			dc.CommandFinishedLocked();
			break;

		case DecoderCommand::NONE:
			dc.Wait();
			break;
		}
	} while (dc.command != DecoderCommand::NONE || !dc.quit);
}

void
decoder_thread_start(DecoderControl &dc)
{
	assert(!dc.thread.IsDefined());

	dc.quit = false;
	dc.thread.Start(decoder_task, &dc);
}
