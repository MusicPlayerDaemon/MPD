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
#include "../ProxyInputStream.hxx"
#include "Log.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "util/ASCII.hxx"
#include "util/Alloc.hxx"
#include "util/ScopeExit.hxx"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

class YoutubeInputStream final : public ProxyInputStream {
	std::string stream_name;

public:
	YoutubeInputStream(const char *_uri, Mutex &_mutex) :
		ProxyInputStream(_uri, _mutex)
	{
		/* lazy way to prevent command injection */
		if(strchr(_uri, '\'')) {
			throw std::runtime_error("Invalid url");
		}

		char *cmd = xstrcatdup("youtube-dl --no-playlist --extract-audio --get-url --get-title --youtube-skip-dash-manifest '", _uri, "'");
		FILE *stream = popen(cmd, "r");
		free(cmd);

		if(!stream) {
			throw std::runtime_error("Can't start youtube-dl");
		}

		char *line = nullptr;
		size_t line_size = 0;
		ssize_t read = 0;
		AtScopeExit(line) { free(line); };

		/* 1st line is song name */
		read = getline(&line, &line_size, stream);
		if(read > 0 && line[read-1] == '\n') line[read-1] = '\0';
		stream_name.assign(line);

		/* 2nd line is stream url */
		read = getline(&line, &line_size, stream);
		if(read > 0 && line[read-1] == '\n') line[read-1] = '\0';

		int status = WEXITSTATUS(pclose(stream));
		if(status != 0) {
			throw std::runtime_error("youtube-dl error");
		}

		SetInput(OpenCurlInputStream(line, {}, mutex));
	}

	std::unique_ptr<Tag> ReadTag() noexcept override {
		TagBuilder builder;

		auto input_tag = ProxyInputStream::ReadTag();
		if(input_tag) {
			builder = std::move(*input_tag.release());
		}

		builder.AddItem(TAG_NAME, stream_name.c_str());
		return builder.CommitNew();
	}
};

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
	try {
		return std::make_unique<YoutubeInputStream>(uri, mutex);
	} catch(std::runtime_error &e) {
		Log(LogLevel::ERROR, e);
		return nullptr;
	}
}

const InputPlugin input_plugin_youtube = {
	"youtube",
	input_youtube_prefixes,
	input_youtube_init,
	nullptr,
	input_youtube_open,
	nullptr
};
