/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "DespotifyInputPlugin.hxx"
#include "lib/despotify/DespotifyUtils.hxx"
#include "../InputStream.hxx"
#include "../InputPlugin.hxx"
#include "tag/Tag.hxx"
#include "util/StringUtil.hxx"
#include "Log.hxx"

extern "C" {
#include <despotify.h>
}

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <stdio.h>

class DespotifyInputStream final : public InputStream {
	struct despotify_session *session;
	struct ds_track *track;
	Tag tag;
	struct ds_pcm_data pcm;
	size_t len_available;
	bool eof;

	DespotifyInputStream(const char *_uri,
			     Mutex &_mutex, Cond &_cond,
			     despotify_session *_session,
			     ds_track *_track)
		:InputStream(_uri, _mutex, _cond),
		 session(_session), track(_track),
		 tag(mpd_despotify_tag_from_track(*track)),
		 len_available(0), eof(false) {

		memset(&pcm, 0, sizeof(pcm));

		/* Despotify outputs pcm data */
		SetMimeType("audio/x-mpd-cdda-pcm");
		SetReady();
	}

public:
	~DespotifyInputStream();

	static InputStream *Open(const char *url, Mutex &mutex, Cond &cond,
				 Error &error);

	void Callback(int sig);

	/* virtual methods from InputStream */

	bool IsEOF() override {
		return eof;
	}

	Tag *ReadTag() override {
		if (tag.IsEmpty())
			return nullptr;

		Tag *result = new Tag(std::move(tag));
		tag.Clear();
		return result;
	}

	size_t Read(void *ptr, size_t size, Error &error) override;

private:
	void FillBuffer();
};

inline void
DespotifyInputStream::FillBuffer()
{
	/* Wait until there is data */
	while (1) {
		int rc = despotify_get_pcm(session, &pcm);

		if (rc == 0 && pcm.len) {
			len_available = pcm.len;
			break;
		}

		if (eof == true)
			break;

		if (rc < 0) {
			LogDebug(despotify_domain, "despotify_get_pcm error");
			eof = true;
			break;
		}

		/* Wait a while until next iteration */
		usleep(50 * 1000);
	}
}

inline void
DespotifyInputStream::Callback(int sig)
{
	switch (sig) {
	case DESPOTIFY_NEW_TRACK:
		break;

	case DESPOTIFY_TIME_TELL:
		break;

	case DESPOTIFY_TRACK_PLAY_ERROR:
		LogWarning(despotify_domain, "Track play error");
		eof = true;
		len_available = 0;
		break;

	case DESPOTIFY_END_OF_PLAYLIST:
		eof = true;
		LogDebug(despotify_domain, "End of playlist");
		break;
	}
}

static void callback(gcc_unused struct despotify_session* ds,
		     int sig, gcc_unused void* data, void* callback_data)
{
	DespotifyInputStream *ctx = (DespotifyInputStream *)callback_data;

	ctx->Callback(sig);
}

DespotifyInputStream::~DespotifyInputStream()
{
	mpd_despotify_unregister_callback(callback);
	despotify_free_track(track);
}

inline InputStream *
DespotifyInputStream::Open(const char *url,
			   Mutex &mutex, Cond &cond,
			   gcc_unused Error &error)
{
	if (!StringStartsWith(url, "spt://"))
		return nullptr;

	despotify_session *session = mpd_despotify_get_session();
	if (session == nullptr)
		return nullptr;

	ds_link *ds_link = despotify_link_from_uri(url + 6);
	if (!ds_link) {
		FormatDebug(despotify_domain, "Can't find %s", url);
		return nullptr;
	}
	if (ds_link->type != LINK_TYPE_TRACK) {
		despotify_free_link(ds_link);
		return nullptr;
	}

	ds_track *track = despotify_link_get_track(session, ds_link);
	despotify_free_link(ds_link);
	if (!track)
		return nullptr;

	DespotifyInputStream *ctx =
		new DespotifyInputStream(url, mutex, cond,
					 session, track);

	if (!mpd_despotify_register_callback(callback, ctx)) {
		delete ctx;
		return nullptr;
	}

	if (despotify_play(ctx->session, ctx->track, false) == false) {
		mpd_despotify_unregister_callback(callback);
		delete ctx;
		return nullptr;
	}

	return ctx;
}

static InputStream *
input_despotify_open(const char *url, Mutex &mutex, Cond &cond, Error &error)
{
	return DespotifyInputStream::Open(url, mutex, cond, error);
}

size_t
DespotifyInputStream::Read(void *ptr, size_t read_size,
			   gcc_unused Error &error)
{
	if (len_available == 0)
		FillBuffer();

	size_t to_cpy = std::min(read_size, len_available);
	memcpy(ptr, pcm.buf, to_cpy);
	len_available -= to_cpy;

	offset += to_cpy;

	return to_cpy;
}

const InputPlugin input_plugin_despotify = {
	"despotify",
	nullptr,
	nullptr,
	input_despotify_open,
};
