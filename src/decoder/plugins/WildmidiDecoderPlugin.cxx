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

#include "WildmidiDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "tag/Handler.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringFormat.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "PluginUnavailable.hxx"

#ifdef _WIN32
/* assume WildMidi is built as static library on Windows; without
   this, linking to the static library would fail */
#define WILDMIDI_STATIC
#endif

extern "C" {
#include <wildmidi_lib.h>
}

static constexpr AudioFormat wildmidi_audio_format{48000, SampleFormat::S16, 2};

static bool
wildmidi_init(const ConfigBlock &block)
{
	const AllocatedPath path =
		block.GetPath("config_file",
			      "/etc/timidity/timidity.cfg");

	if (!FileExists(path)) {
		const auto utf8 = path.ToUTF8();
		throw PluginUnavailable(StringFormat<1024>("configuration file does not exist: %s",
							   utf8.c_str()));
	}

#ifdef LIBWILDMIDI_VERSION
	/* WildMidi_ClearError() requires libwildmidi 0.4 */
	WildMidi_ClearError();
	AtScopeExit() { WildMidi_ClearError(); };
#endif

	if (WildMidi_Init(NarrowPath(path),
			  wildmidi_audio_format.sample_rate,
			  0) != 0) {
#ifdef LIBWILDMIDI_VERSION
		/* WildMidi_GetError() requires libwildmidi 0.4 */
		throw PluginUnavailable(WildMidi_GetError());
#else
		throw PluginUnavailable("WildMidi_Init() failed");
#endif
	}

	return true;
}

static void
wildmidi_finish() noexcept
{
	WildMidi_Shutdown();
}

static DecoderCommand
wildmidi_output(DecoderClient &client, midi *wm)
{
#ifdef LIBWILDMIDI_VER_MAJOR
	/* WildMidi 0.4 has switched from "char*" to "int8_t*" */
	int8_t buffer[4096];
#else
	/* pre 0.4 */
	char buffer[4096];
#endif

	int length = WildMidi_GetOutput(wm, buffer, sizeof(buffer));
	if (length <= 0)
		return DecoderCommand::STOP;

	return client.SubmitData(nullptr, buffer, length, 0);
}

static void
wildmidi_file_decode(DecoderClient &client, Path path_fs)
{
	midi *wm;
	const struct _WM_Info *info;

	wm = WildMidi_Open(NarrowPath(path_fs));
	if (wm == nullptr)
		return;

	info = WildMidi_GetInfo(wm);
	if (info == nullptr) {
		WildMidi_Close(wm);
		return;
	}

	const auto duration =
		SongTime::FromScale<uint64_t>(info->approx_total_samples,
					      wildmidi_audio_format.sample_rate);

	client.Ready(wildmidi_audio_format, true, duration);

	DecoderCommand cmd;
	do {
		info = WildMidi_GetInfo(wm);
		if (info == nullptr)
			break;

		cmd = wildmidi_output(client, wm);

		if (cmd == DecoderCommand::SEEK) {
			unsigned long seek_where = client.GetSeekFrame();

			WildMidi_FastSeek(wm, &seek_where);
			client.CommandFinished();
			cmd = DecoderCommand::NONE;
		}

	} while (cmd == DecoderCommand::NONE);

	WildMidi_Close(wm);
}

static bool
wildmidi_scan_file(Path path_fs, TagHandler &handler) noexcept
{
	midi *wm = WildMidi_Open(NarrowPath(path_fs));
	if (wm == nullptr)
		return false;

	const struct _WM_Info *info = WildMidi_GetInfo(wm);
	if (info == nullptr) {
		WildMidi_Close(wm);
		return false;
	}

	handler.OnAudioFormat(wildmidi_audio_format);

	const auto duration =
		SongTime::FromScale<uint64_t>(info->approx_total_samples,
					      wildmidi_audio_format.sample_rate);
	handler.OnDuration(duration);

	WildMidi_Close(wm);

	return true;
}

static const char *const wildmidi_suffixes[] = {
	"mid",
	nullptr
};

constexpr DecoderPlugin wildmidi_decoder_plugin =
	DecoderPlugin("wildmidi", wildmidi_file_decode, wildmidi_scan_file)
	.WithInit(wildmidi_init, wildmidi_finish)
	.WithSuffixes(wildmidi_suffixes);
