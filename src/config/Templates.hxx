// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CONFIG_TEMPLATES_HXX
#define MPD_CONFIG_TEMPLATES_HXX

struct ConfigTemplate {
	const char *const name;
	const bool repeatable;

	const bool deprecated;

	constexpr ConfigTemplate(const char *_name,
				 bool _repeatable=false,
				 bool _deprecated=false)
		:name(_name), repeatable(_repeatable),
		 deprecated(_deprecated) {}
};

extern const ConfigTemplate config_param_templates[];
extern const ConfigTemplate config_block_templates[];

#endif
