/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "system/FatalError.hxx"
#include "MusicPipe.hxx"
#include "fs/Traits.hxx"
#include "fs/AllocatedPath.hxx"
#include "DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"
#include "DecoderList.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "thread/Name.hxx"
#include "tag/ApeReplayGain.hxx"
#include "Log.hxx"

#include <functional>

static constexpr Domain decoder_thread_domain("decoder_thread");

/**
 * Marks the current decoder command as "finished" and notifies the
 * player thread.
 *
 * @param dc the #DecoderControl object; must be locked
 */
static void
decoder_command_finished_locked(DecoderControl &dc)
{
	assert(dc.command != DecoderCommand::NONE);

	dc.command = DecoderCommand::NONE;

	dc.client_cond.signal();
}

/**
 * Opens the input stream with input_stream::Open(), and waits until
 * the stream gets ready.  If a decoder STOP command is received
 * during that, it cancels the operation (but does not close the
 * stream).
 *
 * Unlock the decoder before calling this function.
 *
 * @return an input_stream on success or if #DecoderCommand::STOP is
 * received, nullptr on error
 */
static InputStream *
decoder_input_stream_open(DecoderControl &dc, const char *uri)
{
	Error error;

	InputStream *is = InputStream::Open(uri, dc.mutex, dc.cond, error);
	if (is == nullptr) {
		if (error.IsDefined())
			LogError(error);

		return nullptr;
	}

	/* wait for the input stream to become ready; its metadata
	   will be available then */

	dc.Lock();

	is->Update();
	while (!is->IsReady() &&
	       dc.command != DecoderCommand::STOP) {
		dc.Wait();

		is->Update();
	}

	if (!is->Check(error)) {
		dc.Unlock();

		LogError(error);
		return nullptr;
	}

	dc.Unlock();

	return is;
}

static InputStream *
decoder_input_stream_open(DecoderControl &dc, Path path)
{
	Error error;

	InputStream *is = OpenLocalInputStream(path, dc.mutex, dc.cond, error);
	if (is == nullptr) {
		LogError(error);
		return nullptr;
	}

	assert(is->IsReady());

	return is;
}

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
		return true;

	/* rewind the stream, so each plugin gets a fresh start */
	input_stream.Rewind(IgnoreError());

	decoder.dc.Unlock();

	FormatThreadName("decoder:%s", plugin.name);

	plugin.StreamDecode(decoder, input_stream);

	SetThreadName("decoder");

	decoder.dc.Lock();

	assert(decoder.dc.state == DecoderState::START ||
	       decoder.dc.state == DecoderState::DECODE);

	return decoder.dc.state != DecoderState::START;
}

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
		return true;

	decoder.dc.Unlock();

	FormatThreadName("decoder:%s", plugin.name);

	plugin.FileDecode(decoder, path);

	SetThreadName("decoder");

	decoder.dc.Lock();

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
	return mime_type != nullptr && plugin.SupportsMimeType(mime_type);
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

	plugin = decoder_plugin_from_name("mad");
	return plugin != nullptr && plugin->stream_decode != nullptr &&
		decoder_stream_decode(*plugin, decoder, is);
}

/**
 * Try decoding a stream.
 */
static bool
decoder_run_stream(Decoder &decoder, const char *uri)
{
	DecoderControl &dc = decoder.dc;
	InputStream *input_stream;
	bool success;

	dc.Unlock();

	input_stream = decoder_input_stream_open(dc, uri);
	if (input_stream == nullptr) {
		dc.Lock();
		return false;
	}

	dc.Lock();

	bool tried = false;
	success = dc.command == DecoderCommand::STOP ||
		decoder_run_stream_locked(decoder, *input_stream, uri,
					  tried) ||
		/* fallback to mp3: this is needed for bastard streams
		   that don't have a suffix or set the mimeType */
		(!tried &&
		 decoder_run_stream_fallback(decoder, *input_stream));

	dc.Unlock();
	delete input_stream;
	dc.Lock();

	return success;
}

/**
 * Attempt to load replay gain data, and pass it to
 * decoder_replay_gain().
 */
static void
decoder_load_replay_gain(Decoder &decoder, Path path_fs)
{
	ReplayGainInfo info;
	if (replay_gain_ape_read(path_fs, info))
		decoder_replay_gain(decoder, &info);
}

static bool
TryDecoderFile(Decoder &decoder, Path path_fs, const char *suffix,
	       const DecoderPlugin &plugin)
{
	if (!plugin.SupportsSuffix(suffix))
		return false;

	DecoderControl &dc = decoder.dc;

	if (plugin.file_decode != nullptr) {
		dc.Lock();

		if (decoder_file_decode(plugin, decoder, path_fs))
			return true;

		dc.Unlock();
	} else if (plugin.stream_decode != nullptr) {
		InputStream *input_stream =
			decoder_input_stream_open(dc, path_fs);
		if (input_stream == nullptr)
			return false;

		dc.Lock();

		bool success = decoder_stream_decode(plugin, decoder,
						     *input_stream);

		dc.Unlock();

		delete input_stream;

		if (success) {
			dc.Lock();
			return true;
		}
	}

	return false;
}

/**
 * Try decoding a file.
 */
static bool
decoder_run_file(Decoder &decoder, const char *uri_utf8, Path path_fs)
{
	const char *suffix = uri_get_suffix(uri_utf8);
	if (suffix == nullptr)
		return false;

	DecoderControl &dc = decoder.dc;
	dc.Unlock();

	decoder_load_replay_gain(decoder, path_fs);

	if (decoder_plugins_try([&decoder, path_fs,
				 suffix](const DecoderPlugin &plugin){
				return TryDecoderFile(decoder,
						      path_fs, suffix,
						      plugin);
			}))
		return true;

	dc.Lock();
	return false;
}

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
	int ret;

	dc.state = DecoderState::START;

	decoder_command_finished_locked(dc);

	ret = !path_fs.IsNull()
		? decoder_run_file(decoder, uri, path_fs)
		: decoder_run_stream(decoder, uri);

	dc.Unlock();

	/* flush the last chunk */

	if (decoder.chunk != nullptr)
		decoder.FlushChunk();

	dc.Lock();

	if (decoder.error.IsDefined()) {
		/* copy the Error from sruct Decoder to
		   DecoderControl */
		dc.state = DecoderState::ERROR;
		dc.error = std::move(decoder.error);
	} else if (ret)
		dc.state = DecoderState::STOP;
	else {
		dc.state = DecoderState::ERROR;

		const char *error_uri = song.GetURI();
		const std::string allocated = uri_remove_auth(error_uri);
		if (!allocated.empty())
			error_uri = allocated.c_str();

		dc.error.Format(decoder_domain,
				 "Failed to decode %s", error_uri);
	}

	dc.client_cond.signal();
}

static void
decoder_run(DecoderControl &dc)
{
	dc.ClearError();

	assert(dc.song != nullptr);
	const DetachedSong &song = *dc.song;

	const char *const uri_utf8 = song.GetRealURI();

	Path path_fs = Path::Null();
	AllocatedPath path_buffer = AllocatedPath::Null();
	if (PathTraitsUTF8::IsAbsolute(uri_utf8)) {
		path_buffer = AllocatedPath::FromUTF8(uri_utf8, dc.error);
		if (path_buffer.IsNull()) {
			dc.state = DecoderState::ERROR;
			decoder_command_finished_locked(dc);
			return;
		}

		path_fs = path_buffer;
	}

	decoder_run_song(dc, song, uri_utf8, path_fs);

}

static void
decoder_task(void *arg)
{
	DecoderControl &dc = *(DecoderControl *)arg;

	SetThreadName("decoder");

	dc.Lock();

	do {
		assert(dc.state == DecoderState::STOP ||
		       dc.state == DecoderState::ERROR);

		switch (dc.command) {
		case DecoderCommand::START:
			dc.CycleMixRamp();
			dc.replay_gain_prev_db = dc.replay_gain_db;
			dc.replay_gain_db = 0;

			decoder_run(dc);
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
			decoder_command_finished_locked(dc);
			break;

		case DecoderCommand::NONE:
			dc.Wait();
			break;
		}
	} while (dc.command != DecoderCommand::NONE || !dc.quit);

	dc.Unlock();
}

void
decoder_thread_start(DecoderControl &dc)
{
	assert(!dc.thread.IsDefined());

	dc.quit = false;

	Error error;
	if (!dc.thread.Start(decoder_task, &dc, error))
		FatalError(error);
}
