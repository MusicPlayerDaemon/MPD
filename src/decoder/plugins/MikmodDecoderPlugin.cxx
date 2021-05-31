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
#include "MikmodDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "tag/Handler.hxx"
#include "fs/Path.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringView.hxx"
#include "Log.hxx"
#include "Version.h"

#include <mikmod.h>

#include <cassert>

static constexpr Domain mikmod_domain("mikmod");

/* this is largely copied from alsaplayer */

static constexpr size_t MIKMOD_FRAME_SIZE = 4096;

static BOOL
mikmod_mpd_init()
{
	return VC_Init();
}

static void
mikmod_mpd_exit()
{
	VC_Exit();
}

static void
mikmod_mpd_update()
{
}

static BOOL
mikmod_mpd_is_present()
{
	return true;
}

static constexpr char drv_name[] = PACKAGE_NAME;
static constexpr char drv_version[] = VERSION;
static constexpr char drv_alias[] = PACKAGE;

static MDRIVER drv_mpd = {
	nullptr,
	drv_name,
	drv_version,
	0,
	255,
	drv_alias,
	nullptr,  /* CmdLineHelp */
	nullptr,  /* CommandLine */
	mikmod_mpd_is_present,
	VC_SampleLoad,
	VC_SampleUnload,
	VC_SampleSpace,
	VC_SampleLength,
	mikmod_mpd_init,
	mikmod_mpd_exit,
	nullptr,
	VC_SetNumVoices,
	VC_PlayStart,
	VC_PlayStop,
	mikmod_mpd_update,
	nullptr,
	VC_VoiceSetVolume,
	VC_VoiceGetVolume,
	VC_VoiceSetFrequency,
	VC_VoiceGetFrequency,
	VC_VoiceSetPanning,
	VC_VoiceGetPanning,
	VC_VoicePlay,
	VC_VoiceStop,
	VC_VoiceStopped,
	VC_VoiceGetPosition,
	VC_VoiceRealVolume
};

static bool mikmod_loop;
static unsigned mikmod_sample_rate;

static bool
mikmod_decoder_init(const ConfigBlock &block)
{
	static char params[] = "";

	mikmod_loop = block.GetBlockValue("loop", false);
	mikmod_sample_rate = block.GetPositiveValue("sample_rate", 44100U);
	if (!audio_valid_sample_rate(mikmod_sample_rate))
		throw FormatRuntimeError("Invalid sample rate in line %d: %u",
					 block.line, mikmod_sample_rate);

	md_device = 0;
	md_reverb = 0;

	MikMod_RegisterDriver(&drv_mpd);
	MikMod_RegisterAllLoaders();

	md_pansep = 64;
	md_mixfreq = mikmod_sample_rate;
	md_mode = (DMODE_SOFT_MUSIC | DMODE_INTERP | DMODE_STEREO |
		   DMODE_16BITS);

	if (MikMod_Init(params)) {
		FmtError(mikmod_domain,
			 "Could not init MikMod: {}",
			 MikMod_strerror(MikMod_errno));
		return false;
	}

	return true;
}

static void
mikmod_decoder_finish() noexcept
{
	MikMod_Exit();
}

static void
mikmod_decoder_file_decode(DecoderClient &client, Path path_fs)
{
	/* deconstify the path because libmikmod wants a non-const
	   string pointer */
	char *const path2 = const_cast<char *>(path_fs.c_str());

	MODULE *handle;
	int ret;
	SBYTE buffer[MIKMOD_FRAME_SIZE];

	handle = Player_Load(path2, 128, 0);

	if (handle == nullptr) {
		FmtError(mikmod_domain, "failed to open mod: {}", path_fs);
		return;
	}

	handle->loop = mikmod_loop;

	const AudioFormat audio_format(mikmod_sample_rate, SampleFormat::S16, 2);
	assert(audio_format.IsValid());

	client.Ready(audio_format, false, SignedSongTime::Negative());

	Player_Start(handle);

	DecoderCommand cmd = DecoderCommand::NONE;
	while (cmd == DecoderCommand::NONE && Player_Active()) {
		ret = VC_WriteBytes(buffer, sizeof(buffer));
		cmd = client.SubmitData(nullptr, buffer, ret, 0);
	}

	Player_Stop();
	Player_Free(handle);
}

static bool
mikmod_decoder_scan_file(Path path_fs, TagHandler &handler) noexcept
{
	/* deconstify the path because libmikmod wants a non-const
	   string pointer */
	char *const path2 = const_cast<char *>(path_fs.c_str());

	MODULE *handle = Player_Load(path2, 128, 0);

	if (handle == nullptr) {
		FmtDebug(mikmod_domain, "Failed to open file: {}", path_fs);
		return false;
	}

	Player_Free(handle);

	char *title = Player_LoadTitle(path2);
	if (title != nullptr) {
		handler.OnTag(TAG_TITLE, title);
		MikMod_free(title);
	}

	return true;
}

static const char *const mikmod_decoder_suffixes[] = {
	"amf",
	"dsm",
	"far",
	"gdm",
	"imf",
	"it",
	"med",
	"mod",
	"mtm",
	"s3m",
	"stm",
	"stx",
	"ult",
	"uni",
	"xm",
	nullptr
};

constexpr DecoderPlugin mikmod_decoder_plugin =
	DecoderPlugin("mikmod",
		      mikmod_decoder_file_decode, mikmod_decoder_scan_file)
	.WithInit(mikmod_decoder_init, mikmod_decoder_finish)
	.WithSuffixes(mikmod_decoder_suffixes);
