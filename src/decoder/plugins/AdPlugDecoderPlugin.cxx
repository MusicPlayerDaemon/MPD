/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "AdPlugDecoderPlugin.h"
#include "tag/TagHandler.hxx"
#include "../DecoderAPI.hxx"
#include "CheckAudioFormat.hxx"
#include "fs/Path.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "util/Macros.hxx"
#include "Log.hxx"

#include <adplug/adplug.h>
#include <adplug/emuopl.h>

#include <assert.h>

static constexpr Domain adplug_domain("adplug");

static unsigned sample_rate;

static bool
adplug_init(const config_param &param)
{
	FormatDebug(adplug_domain, "adplug %s",
		    CAdPlug::get_version().c_str());

	Error error;

	sample_rate = param.GetBlockValue("sample_rate", 48000u);
	if (!audio_check_sample_rate(sample_rate, error)) {
		LogError(error);
		return false;
	}

	return true;
}

static void
adplug_file_decode(Decoder &decoder, Path path_fs)
{
	CEmuopl opl(sample_rate, true, true);
	opl.init();

	CPlayer *player = CAdPlug::factory(path_fs.c_str(), &opl);
	if (player == nullptr)
		return;

	const AudioFormat audio_format(sample_rate, SampleFormat::S16, 2);
	assert(audio_format.IsValid());

	decoder_initialized(decoder, audio_format, false,
			    SongTime::FromMS(player->songlength()));

	DecoderCommand cmd;

	do {
		if (!player->update())
			break;

		int16_t buffer[2048];
		constexpr unsigned frames_per_buffer = ARRAY_SIZE(buffer) / 2;
		opl.update(buffer, frames_per_buffer);
		cmd = decoder_data(decoder, nullptr,
				   buffer, sizeof(buffer),
				   0);
	} while (cmd == DecoderCommand::NONE);

	delete player;
}

static void
adplug_scan_tag(TagType type, const std::string &value,
		const struct tag_handler *handler, void *handler_ctx)
{
	if (!value.empty())
		tag_handler_invoke_tag(handler, handler_ctx,
				       type, value.c_str());
}

static bool
adplug_scan_file(Path path_fs,
		 const struct tag_handler *handler, void *handler_ctx)
{
	CEmuopl opl(sample_rate, true, true);
	opl.init();

	CPlayer *player = CAdPlug::factory(path_fs.c_str(), &opl);
	if (player == nullptr)
		return false;

	tag_handler_invoke_duration(handler, handler_ctx,
				    SongTime::FromMS(player->songlength()));

	if (handler->tag != nullptr) {
		adplug_scan_tag(TAG_TITLE, player->gettitle(),
				handler, handler_ctx);
		adplug_scan_tag(TAG_ARTIST, player->getauthor(),
				handler, handler_ctx);
		adplug_scan_tag(TAG_COMMENT, player->getdesc(),
				handler, handler_ctx);
	}

	delete player;
	return true;
}

static const char *const adplug_suffixes[] = {
	"amd",
	"d00",
	"hsc",
	"laa",
	"rad",
	"raw",
	"sa2",
	nullptr
};

const struct DecoderPlugin adplug_decoder_plugin = {
	"adplug",
	adplug_init,
	nullptr,
	nullptr,
	adplug_file_decode,
	adplug_scan_file,
	nullptr,
	nullptr,
	adplug_suffixes,
	nullptr,
};
