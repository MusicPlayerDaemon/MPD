/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "config/Data.hxx"
#include "event/Thread.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "input/Init.hxx"
#include "input/InputStream.hxx"
#include "tag/Handler.hxx"
#include "tag/Generic.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringBuffer.hxx"
#include "util/StringView.hxx"
#include "util/PrintException.hxx"

#include <cassert>
#include <stdexcept>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

class DumpTagHandler final : public NullTagHandler {
	bool empty = true;

public:
	DumpTagHandler() noexcept
		:NullTagHandler(WANT_DURATION|WANT_TAG|WANT_PAIR|WANT_PICTURE) {}

	bool IsEmpty() const noexcept {
		return empty;
	}

	void OnDuration(SongTime duration) noexcept override {
		printf("duration=%f\n", duration.ToDoubleS());
	}

	void OnTag(TagType type, StringView value) noexcept override {
		printf("[%s]=%.*s\n", tag_item_names[type],
		       int(value.size), value.data);
		empty = false;
	}

	void OnPair(StringView key, StringView value) noexcept override {
		printf("\"%.*s\"=%.*s\n",
		       int(key.size), key.data,
		       int(value.size), value.data);
	}

	void OnAudioFormat(AudioFormat af) noexcept override {
		printf("%s\n", ToString(af).c_str());
	}

	void OnPicture(const char *mime_type,
		       ConstBuffer<void> buffer) noexcept override {
		printf("picture mime='%s' size=%zu\n", mime_type, buffer.size);
	}
};

int main(int argc, char **argv)
try {
	const char *decoder_name;
	const struct DecoderPlugin *plugin;

#ifdef HAVE_LOCALE_H
	/* initialize locale */
	setlocale(LC_CTYPE,"");
#endif

	if (argc != 3) {
		fprintf(stderr, "Usage: read_tags DECODER FILE\n");
		return EXIT_FAILURE;
	}

	decoder_name = argv[1];
	const char *path = argv[2];

	EventThread io_thread;
	io_thread.Start();

	const ScopeInputPluginsInit input_plugins_init(ConfigData(),
						       io_thread.GetEventLoop());

	const ScopeDecoderPluginsInit decoder_plugins_init({});

	plugin = decoder_plugin_from_name(decoder_name);
	if (plugin == nullptr) {
		fprintf(stderr, "No such decoder: %s\n", decoder_name);
		return EXIT_FAILURE;
	}

	DumpTagHandler h;
	bool success;
	try {
		success = plugin->ScanFile(FromNarrowPath(path), h);
	} catch (...) {
		PrintException(std::current_exception());
		success = false;
	}

	Mutex mutex;
	InputStreamPtr is;

	if (!success && plugin->scan_stream != nullptr) {
		is = InputStream::OpenReady(path, mutex);
		success = plugin->ScanStream(*is, h);
	}

	if (!success) {
		fprintf(stderr, "Failed to read tags\n");
		return EXIT_FAILURE;
	}

	if (h.IsEmpty()) {
		if (is)
			ScanGenericTags(*is, h);
		else
			ScanGenericTags(FromNarrowPath(path), h);
	}

	return 0;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
