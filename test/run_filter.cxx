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

#include "ConfigGlue.hxx"
#include "fs/Path.hxx"
#include "AudioParser.hxx"
#include "AudioFormat.hxx"
#include "filter/LoadOne.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "pcm/Volume.hxx"
#include "mixer/MixerControl.hxx"
#include "system/Error.hxx"
#include "system/FileDescriptor.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringBuffer.hxx"
#include "util/RuntimeError.hxx"
#include "util/PrintException.hxx"

#include <memory>
#include <stdexcept>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void
mixer_set_volume(gcc_unused Mixer *mixer,
		 gcc_unused unsigned volume)
{
}

static std::unique_ptr<PreparedFilter>
LoadFilter(const ConfigData &config, const char *name)
{
	const auto *param = config.FindBlock(ConfigBlockOption::AUDIO_FILTER,
					     "name", name);
	if (param == NULL)
		throw FormatRuntimeError("No such configured filter: %s",
					 name);

	return filter_configured_new(*param);
}

static size_t
ReadOrThrow(FileDescriptor fd, void *buffer, size_t size)
{
	auto nbytes = fd.Read(buffer, size);
	if (nbytes < 0)
		throw MakeErrno("Read failed");

	return nbytes;
}

static size_t
WriteOrThrow(FileDescriptor fd, const void *buffer, size_t size)
{
	auto nbytes = fd.Write(buffer, size);
	if (nbytes < 0)
		throw MakeErrno("Write failed");

	return nbytes;
}

static void
FullRead(FileDescriptor fd, void *_buffer, size_t size)
{
	auto buffer = (uint8_t *)_buffer;

	while (size > 0) {
		size_t nbytes = ReadOrThrow(fd, buffer, size);
		if (nbytes == 0)
			throw std::runtime_error("Premature end of input");

		buffer += nbytes;
		size -= nbytes;
	}
}

static void
FullWrite(FileDescriptor fd, ConstBuffer<uint8_t> src)
{
	while (!src.empty()) {
		size_t nbytes = WriteOrThrow(fd, src.data, src.size);
		if (nbytes == 0)
			throw std::runtime_error("Write failed");

		src.skip_front(nbytes);
	}
}

static void
FullWrite(FileDescriptor fd, ConstBuffer<void> src)
{
	FullWrite(fd, ConstBuffer<uint8_t>::FromVoid(src));
}

static size_t
ReadFrames(FileDescriptor fd, void *_buffer, size_t size, size_t frame_size)
{
	auto buffer = (uint8_t *)_buffer;

	size = (size / frame_size) * frame_size;

	size_t nbytes = ReadOrThrow(fd, buffer, size);

	const size_t modulo = nbytes % frame_size;
	if (modulo > 0) {
		size_t rest = frame_size - modulo;
		FullRead(fd, buffer + nbytes, rest);
		nbytes += rest;
	}

	return nbytes;
}

int main(int argc, char **argv)
try {
	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Usage: run_filter CONFIG NAME [FORMAT] <IN\n");
		return EXIT_FAILURE;
	}

	const Path config_path = Path::FromFS(argv[1]);

	AudioFormat audio_format(44100, SampleFormat::S16, 2);

	/* read configuration file (mpd.conf) */

	const auto config = AutoLoadConfigFile(config_path);

	/* parse the audio format */

	if (argc > 3)
		audio_format = ParseAudioFormat(argv[3], false);

	const size_t in_frame_size = audio_format.GetFrameSize();

	/* initialize the filter */

	auto prepared_filter = LoadFilter(config, argv[2]);

	/* open the filter */

	auto filter = prepared_filter->Open(audio_format);

	const AudioFormat out_audio_format = filter->GetOutAudioFormat();

	fprintf(stderr, "audio_format=%s\n",
		ToString(out_audio_format).c_str());

	/* play */

	FileDescriptor input_fd(STDIN_FILENO);
	FileDescriptor output_fd(STDOUT_FILENO);

	while (true) {
		char buffer[4096];

		ssize_t nbytes = ReadFrames(input_fd, buffer, sizeof(buffer),
					    in_frame_size);
		if (nbytes == 0)
			break;

		auto dest = filter->FilterPCM({(const void *)buffer, (size_t)nbytes});
		FullWrite(output_fd, dest);
	}

	while (true) {
		auto dest = filter->Flush();
		if (dest.IsNull())
			break;
		FullWrite(output_fd, dest);
	}

	/* cleanup and exit */

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
