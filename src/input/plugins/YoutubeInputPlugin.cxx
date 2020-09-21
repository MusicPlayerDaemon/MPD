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

#include "YoutubeInputPlugin.hxx"
#include "CurlInputPlugin.hxx"
#include "PluginUnavailable.hxx"
#include "../InputPlugin.hxx"
#include "../InputStream.hxx"
#include "../TaggedInputStream.hxx"
#include "Chrono.hxx"
#include "Log.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "util/ScopeExit.hxx"
#include "util/ExecOpen.hxx"
#include "util/Domain.hxx"

#include <yajl/yajl_tree.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const Domain youtube_domain("youtube");

static const char *input_youtube_prefixes[] = {
	"https",
	nullptr
};

static void
input_youtube_init(EventLoop &, const ConfigBlock &)
{
	if(WEXITSTATUS(system("youtube-dl --version > /dev/null")) != 0)
		throw PluginUnavailable("youtube-dl not found");
}

static InputStreamPtr
input_youtube_open(const char *uri, Mutex &mutex)
{
	const char *args[] = {
		"youtube-dl",
		"--no-playlist",
		"--format=bestaudio",
		"--dump-single-json",
		uri,
		nullptr
	};

	int pid;
	FILE *stream = exec_open(&pid, "youtube-dl", args);
	if(!stream) {
		LogErrno(youtube_domain, "Can't spawn youtube-dl");
		return nullptr;
	}

	/* Read youtube-dl output */
	std::string json;
	char buf[512];
	ssize_t size = 0;
	while(true) {
		size = fread(buf, sizeof(char), 512, stream);
		if(size > 0)
			json.append(buf, size);
		else
			break;
	}

	fclose(stream);
	int status = exec_wait(pid);
	if(status != 0) {
		FormatError(youtube_domain, "youtube-dl returned %d", status);
		return nullptr;
	}

	/* Parse json */
	yajl_val root = yajl_tree_parse(json.c_str(), nullptr, 0);

	if(!root) {
		LogError(youtube_domain, "Failed to parse youtube-dl output");
		return nullptr;
	}

	AtScopeExit(root) {
		yajl_tree_free(root);
	};

	TagBuilder tag_builder;

	/* Get song name */
	{
		const char *path[] = { "title", nullptr };
		char *name = YAJL_GET_STRING(yajl_tree_get(root, path, yajl_t_string));

		if(name) {
			tag_builder.AddItem(TAG_NAME, name);
		}
	}

	/* Get duration */
	{
		const char *path[] = { "duration", nullptr };

		yajl_val duration = yajl_tree_get(root, path, yajl_t_number);

		if(YAJL_IS_NUMBER(duration)) {
			tag_builder.SetDuration(
				SignedSongTime(std::chrono::seconds(YAJL_GET_INTEGER(duration)))
			);
		}
	}

	/* Get url */
	/* get format id first */
	const char *format = nullptr;
	{
		const char *path[] = { "format_id", nullptr };
		format = YAJL_GET_STRING(yajl_tree_get(root, path, yajl_t_string));

		if(!format) {
			LogError(youtube_domain, "Can't get format id");
			return nullptr;
		}
	}

	/* then get url using format id */
	const char *song_url = nullptr;
	{
		const char
			*formats_path[] = { "formats", nullptr },
			*format_path[]  = { "format_id", nullptr },
			*url_path[]     = { "url", nullptr };

		auto *format_arr = YAJL_GET_ARRAY(yajl_tree_get(root, formats_path, yajl_t_array));
		if(!format_arr) {
			LogError(youtube_domain, "Can't get formats");
			return nullptr;
		}

		for(size_t i = 0; i < format_arr->len; i++) {
			char *val_format = YAJL_GET_STRING(yajl_tree_get(format_arr->values[i], format_path, yajl_t_string));
			if(val_format && strcmp(val_format, format)) {
				song_url = YAJL_GET_STRING(yajl_tree_get(format_arr->values[i], url_path, yajl_t_string));
				break;
			}
		}

		if(!song_url) {
			LogError(youtube_domain, "Can't get url");
			return nullptr;
		}
	}

	return std::make_unique<TaggedInputStream>(
		OpenCurlInputStream(song_url, {}, mutex),
		std::unique_ptr<Tag>(tag_builder.CommitNew())
	);
}

const InputPlugin input_plugin_youtube = {
	"youtube",
	input_youtube_prefixes,
	input_youtube_init,
	nullptr,
	input_youtube_open,
	nullptr
};
