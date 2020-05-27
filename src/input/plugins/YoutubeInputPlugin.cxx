/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

/* necessary because libavutil/common.h uses UINT64_C */
#define __STDC_CONSTANT_MACROS

#include "YoutubeInputPlugin.hxx"
#include "CurlInputPlugin.hxx"
#include "PluginUnavailable.hxx"
#include "../InputPlugin.hxx"
#include "../InputStream.hxx"
#include "util/ASCII.hxx"
#include "util/Alloc.hxx"
#include "util/ScopeExit.hxx"

#include <cstdio>
#include <cstdlib>

static const char *input_youtube_prefixes[] = {
	"youtube",
	nullptr
};

static char *video_url = nullptr;
static size_t url_length = 0;

static void
input_youtube_init(EventLoop &, const ConfigBlock &)
{
	if(WEXITSTATUS(system("which youtube-dl")) != 0)
		throw PluginUnavailable("youtube-dl not found");
}

static void
input_youtube_finish()
{
	free(video_url);
	video_url = nullptr;
}

static InputStreamPtr
input_youtube_open(const char *uri, Mutex &mutex)
{
	assert(StringEqualsCaseASCII(uri, "youtube://", 10));
	uri += 10;

	char *cmd = xstrcatdup("youtube-dl --extract-audio --get-url --youtube-skip-dash-manifest https://", uri);
	FILE *stream = popen(cmd, "r");
	free(cmd);

	if(!stream) return nullptr;

	ssize_t read = getline(&video_url, &url_length, stream);
	int   status = WEXITSTATUS(pclose(stream));

	if(status != 0 || read < 0) return nullptr;

	/* Remove newline from the url */
	if(video_url[url_length-2] == '\n')
		video_url[url_length-2] = '\0';

	return OpenCurlInputStream(video_url, {}, mutex);
}

const InputPlugin input_plugin_youtube = {
	"youtube",
	input_youtube_prefixes,
	input_youtube_init,
	input_youtube_finish,
	input_youtube_open,
	nullptr
};

// vim: set noexpandtab
