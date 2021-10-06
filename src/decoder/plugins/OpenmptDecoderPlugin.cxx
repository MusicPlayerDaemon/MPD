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

#include "OpenmptDecoderPlugin.hxx"
#include "decoder/Features.h"
#include "ModCommon.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "tag/Handler.hxx"
#include "tag/Type.h"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringView.hxx"
#include "Log.hxx"

#include <libopenmpt/libopenmpt.hpp>

#include <cassert>

static constexpr Domain openmpt_domain("openmpt");

static constexpr size_t OPENMPT_FRAME_SIZE = 4096;
static constexpr int32_t OPENMPT_SAMPLE_RATE = 48000;

static int openmpt_repeat_count;
static int openmpt_stereo_separation;
static int openmpt_interpolation_filter;
static bool openmpt_override_mptm_interp_filter;
static int openmpt_volume_ramping;
static bool openmpt_sync_samples;
static bool openmpt_emulate_amiga;
#ifdef HAVE_LIBOPENMPT_VERSION_0_5
static std::string_view openmpt_emulate_amiga_type;
#endif

static bool
openmpt_decoder_init(const ConfigBlock &block)
{
	openmpt_repeat_count = block.GetBlockValue("repeat_count", 0);
	openmpt_stereo_separation = block.GetBlockValue("stereo_separation", 100);
	openmpt_interpolation_filter = block.GetBlockValue("interpolation_filter", 0);
	openmpt_override_mptm_interp_filter = block.GetBlockValue("override_mptm_interp_filter", false);
	openmpt_volume_ramping = block.GetBlockValue("volume_ramping", -1);
	openmpt_sync_samples = block.GetBlockValue("sync_samples", true);
	openmpt_emulate_amiga = block.GetBlockValue("emulate_amiga", true);
#ifdef HAVE_LIBOPENMPT_VERSION_0_5
	openmpt_emulate_amiga_type = block.GetBlockValue("emulate_amiga_type", "auto");
#endif

	return true;
}

static void
mod_decode(DecoderClient &client, InputStream &is)
{
	int ret;
	char audio_buffer[OPENMPT_FRAME_SIZE];

	const auto buffer = mod_loadfile(&openmpt_domain, &client, is);
	if (buffer.IsNull()) {
		LogWarning(openmpt_domain, "could not load stream");
		return;
	}

	openmpt::module mod(buffer.data(), buffer.size());

	/* alter settings */
	mod.set_repeat_count(openmpt_repeat_count);
	mod.set_render_param(mod.RENDER_STEREOSEPARATION_PERCENT, openmpt_stereo_separation);
	mod.set_render_param(mod.RENDER_INTERPOLATIONFILTER_LENGTH, openmpt_interpolation_filter);
	if (!openmpt_override_mptm_interp_filter && mod.get_metadata("type") == "mptm") {
		/* The MPTM format has a setting for which interpolation filter should be used.
		 * If we want to play the module back the way the composer intended it,
		 * we have to set the interpolation filter setting in libopenmpt back to 0: internal default. */
		mod.set_render_param(mod.RENDER_INTERPOLATIONFILTER_LENGTH, 0);
	}
	mod.set_render_param(mod.RENDER_VOLUMERAMPING_STRENGTH, openmpt_volume_ramping);
#ifdef HAVE_LIBOPENMPT_VERSION_0_5
	mod.ctl_set_boolean("seek.sync_samples", openmpt_sync_samples);
	mod.ctl_set_boolean("render.resampler.emulate_amiga", openmpt_emulate_amiga);
	mod.ctl_set_text("render.resampler.emulate_amiga_type", openmpt_emulate_amiga_type);
#else
	mod.ctl_set("seek.sync_samples", std::to_string((unsigned)openmpt_sync_samples));
	mod.ctl_set("render.resampler.emulate_amiga", std::to_string((unsigned)openmpt_emulate_amiga));
#endif

	static constexpr AudioFormat audio_format(OPENMPT_SAMPLE_RATE, SampleFormat::FLOAT, 2);
	assert(audio_format.IsValid());

	client.Ready(audio_format, is.IsSeekable(),
		     SongTime::FromS(mod.get_duration_seconds()));

	DecoderCommand cmd;
	do {
		ret = mod.read_interleaved_stereo(OPENMPT_SAMPLE_RATE, OPENMPT_FRAME_SIZE / 2 / sizeof(float), (float*)audio_buffer);
		if (ret <= 0)
			break;

		cmd = client.SubmitData(nullptr,
					audio_buffer, ret * 2 * sizeof(float),
					0);

		if (cmd == DecoderCommand::SEEK) {
			mod.set_position_seconds(client.GetSeekTime().ToS());
			client.CommandFinished();
		}

	} while (cmd != DecoderCommand::STOP);
}

static bool
openmpt_scan_stream(InputStream &is, TagHandler &handler) noexcept
try {
	const auto buffer = mod_loadfile(&openmpt_domain, nullptr, is);
	if (buffer.IsNull()) {
		LogWarning(openmpt_domain, "could not load stream");
		return false;
	}

	openmpt::module mod(buffer.data(), buffer.size());

	handler.OnDuration(SongTime::FromS(mod.get_duration_seconds()));

	/* Tagging */
	handler.OnTag(TAG_TITLE, mod.get_metadata("title").c_str());
	handler.OnTag(TAG_ARTIST, mod.get_metadata("artist").c_str());
	handler.OnTag(TAG_COMMENT, mod.get_metadata("message").c_str());
	handler.OnTag(TAG_DATE, mod.get_metadata("date").c_str());
	handler.OnTag(TAG_PERFORMER, mod.get_metadata("tracker").c_str());

	return true;
} catch (...) {
	/* libopenmpt usually throws openmpt::exception, but "may
	   additionally throw any exception thrown by the standard
	   library which are all derived from std::exception", we
	   let's just catch all here */
	LogError(std::current_exception(), "libopenmpt failed");
	return false;
}

static const char *const mod_suffixes[] = {
	"mptm", "mod", "s3m", "xm", "it", "669", "amf", "ams",
	"c67", "dbm", "digi", "dmf", "dsm", "dtm", "far", "imf",
	"ice", "j2b", "m15", "mdl", "med", "mms", "mt2", "mtm",
	"nst", "okt", "plm", "psm", "pt36", "ptm", "sfx", "sfx2",
	"st26", "stk", "stm", "stp", "ult", "wow", "gdm", "mo3",
	"oxm", "umx", "xpk", "ppm", "mmcmp",
	nullptr
};

constexpr DecoderPlugin openmpt_decoder_plugin =
	DecoderPlugin("openmpt", mod_decode, openmpt_scan_stream)
	.WithInit(openmpt_decoder_init)
	.WithSuffixes(mod_suffixes);
