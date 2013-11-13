/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "Song.hxx"
#include "system/FatalError.hxx"
#include "Mapper.hxx"
#include "fs/Traits.hxx"
#include "fs/AllocatedPath.hxx"
#include "DecoderAPI.hxx"
#include "tag/Tag.hxx"
#include "InputStream.hxx"
#include "DecoderList.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
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
	while (!is->ready &&
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

static bool
decoder_stream_decode(const DecoderPlugin &plugin,
		      Decoder &decoder,
		      InputStream &input_stream)
{
	assert(plugin.stream_decode != nullptr);
	assert(decoder.stream_tag == nullptr);
	assert(decoder.decoder_tag == nullptr);
	assert(input_stream.ready);
	assert(decoder.dc.state == DecoderState::START);

	FormatDebug(decoder_thread_domain, "probing plugin %s", plugin.name);

	if (decoder.dc.command == DecoderCommand::STOP)
		return true;

	/* rewind the stream, so each plugin gets a fresh start */
	input_stream.Rewind(IgnoreError());

	decoder.dc.Unlock();

	plugin.StreamDecode(decoder, input_stream);

	decoder.dc.Lock();

	assert(decoder.dc.state == DecoderState::START ||
	       decoder.dc.state == DecoderState::DECODE);

	return decoder.dc.state != DecoderState::START;
}

static bool
decoder_file_decode(const DecoderPlugin &plugin,
		    Decoder &decoder, const char *path)
{
	assert(plugin.file_decode != nullptr);
	assert(decoder.stream_tag == nullptr);
	assert(decoder.decoder_tag == nullptr);
	assert(path != nullptr);
	assert(PathTraits::IsAbsoluteFS(path));
	assert(decoder.dc.state == DecoderState::START);

	FormatDebug(decoder_thread_domain, "probing plugin %s", plugin.name);

	if (decoder.dc.command == DecoderCommand::STOP)
		return true;

	decoder.dc.Unlock();

	plugin.FileDecode(decoder, path);

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

	return !is.mime.empty() && plugin.SupportsMimeType(is.mime.c_str());
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
	const char *const suffix = uri_get_suffix(uri);

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
	input_stream->Close();
	dc.Lock();

	return success;
}

/**
 * Attempt to load replay gain data, and pass it to
 * decoder_replay_gain().
 */
static void
decoder_load_replay_gain(Decoder &decoder, const char *path_fs)
{
	ReplayGainInfo info;
	if (replay_gain_ape_read(Path::FromFS(path_fs), info))
		decoder_replay_gain(decoder, &info);
}

/**
 * Try decoding a file.
 */
static bool
decoder_run_file(Decoder &decoder, const char *path_fs)
{
	DecoderControl &dc = decoder.dc;
	const char *suffix = uri_get_suffix(path_fs);
	const struct DecoderPlugin *plugin = nullptr;

	if (suffix == nullptr)
		return false;

	dc.Unlock();

	decoder_load_replay_gain(decoder, path_fs);

	while ((plugin = decoder_plugin_from_suffix(suffix, plugin)) != nullptr) {
		if (plugin->file_decode != nullptr) {
			dc.Lock();

			if (decoder_file_decode(*plugin, decoder, path_fs))
				return true;

			dc.Unlock();
		} else if (plugin->stream_decode != nullptr) {
			InputStream *input_stream;
			bool success;

			input_stream = decoder_input_stream_open(dc, path_fs);
			if (input_stream == nullptr)
				continue;

			dc.Lock();

			success = decoder_stream_decode(*plugin, decoder,
							*input_stream);

			dc.Unlock();

			input_stream->Close();

			if (success) {
				dc.Lock();
				return true;
			}
		}
	}

	dc.Lock();
	return false;
}

static void
decoder_run_song(DecoderControl &dc,
		 const Song *song, const char *uri)
{
	Decoder decoder(dc, dc.start_ms > 0,
			song->tag != nullptr && song->IsFile()
			? new Tag(*song->tag) : nullptr);
	int ret;

	dc.state = DecoderState::START;

	decoder_command_finished_locked(dc);

	ret = song->IsFile()
		? decoder_run_file(decoder, uri)
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

		const char *error_uri = song->uri;
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

	const Song *song = dc.song;
	assert(song != nullptr);

	const std::string uri = song->IsFile()
		? std::string(map_song_fs(*song).c_str())
		: song->GetURI();

	if (uri.empty()) {
		dc.state = DecoderState::ERROR;
		dc.error.Set(decoder_domain, "Failed to map song");

		decoder_command_finished_locked(dc);
		return;
	}

	decoder_run_song(dc, song, uri.c_str());

}

static void
decoder_task(void *arg)
{
	DecoderControl &dc = *(DecoderControl *)arg;

	dc.Lock();

	do {
		assert(dc.state == DecoderState::STOP ||
		       dc.state == DecoderState::ERROR);

		switch (dc.command) {
		case DecoderCommand::START:
			dc.CycleMixRamp();
			dc.replay_gain_prev_db = dc.replay_gain_db;
			dc.replay_gain_db = 0;

			/* fall through */

		case DecoderCommand::SEEK:
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
