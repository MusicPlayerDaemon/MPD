// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Defaults.hxx"
#include "config/Data.hxx"

AudioOutputDefaults::AudioOutputDefaults(const ConfigData &config)
	:normalize(config.GetBool(ConfigOption::VOLUME_NORMALIZATION, false)),
	 mixer_type(mixer_type_parse(config.GetString(ConfigOption::MIXER_TYPE,
						      "hardware")))

{
}
