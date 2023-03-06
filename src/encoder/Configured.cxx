// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Configured.hxx"
#include "EncoderList.hxx"
#include "EncoderPlugin.hxx"
#include "config/Block.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/StringAPI.hxx"

static const EncoderPlugin &
GetConfiguredEncoderPlugin(const ConfigBlock &block, bool shout_legacy)
{
	const char *name = block.GetBlockValue("encoder", nullptr);
	if (name == nullptr && shout_legacy)
		name = block.GetBlockValue("encoding", nullptr);

	if (name == nullptr)
		name = "vorbis";

	if (shout_legacy) {
		if (StringIsEqual(name, "ogg"))
			name = "vorbis";
		else if (StringIsEqual(name, "mp3"))
			name = "lame";
	}

	const auto plugin = encoder_plugin_get(name);
	if (plugin == nullptr)
		throw FmtRuntimeError("No such encoder: {}", name);

	return *plugin;
}

PreparedEncoder *
CreateConfiguredEncoder(const ConfigBlock &block, bool shout_legacy)
{
	return encoder_init(GetConfiguredEncoderPlugin(block, shout_legacy),
			    block);
}
