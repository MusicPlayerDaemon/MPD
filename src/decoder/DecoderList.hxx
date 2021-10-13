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

#ifndef MPD_DECODER_LIST_HXX
#define MPD_DECODER_LIST_HXX

#include <string_view>

struct ConfigData;
struct DecoderPlugin;

extern const struct DecoderPlugin *const decoder_plugins[];
extern bool decoder_plugins_enabled[];

/* interface for using plugins */

[[gnu::pure]]
const struct DecoderPlugin *
decoder_plugin_from_name(const char *name) noexcept;

/* this is where we "load" all the "plugins" ;-) */
void
decoder_plugin_init_all(const ConfigData &config);

/* this is where we "unload" all the "plugins" */
void
decoder_plugin_deinit_all() noexcept;

class ScopeDecoderPluginsInit {
public:
	explicit ScopeDecoderPluginsInit(const ConfigData &config) {
		decoder_plugin_init_all(config);
	}

	~ScopeDecoderPluginsInit() noexcept {
		decoder_plugin_deinit_all();
	}
};

template<typename F>
static inline const DecoderPlugin *
decoder_plugins_find(F f) noexcept
{
	for (unsigned i = 0; decoder_plugins[i] != nullptr; ++i)
		if (decoder_plugins_enabled[i] && f(*decoder_plugins[i]))
			return decoder_plugins[i];

	return nullptr;
}

template<typename F>
static inline bool
decoder_plugins_try(F f)
{
	for (unsigned i = 0; decoder_plugins[i] != nullptr; ++i)
		if (decoder_plugins_enabled[i] && f(*decoder_plugins[i]))
			return true;

	return false;
}

template<typename F>
static inline void
decoder_plugins_for_each(F f)
{
	for (auto i = decoder_plugins; *i != nullptr; ++i)
		f(**i);
}

template<typename F>
static inline void
decoder_plugins_for_each_enabled(F f)
{
	for (unsigned i = 0; decoder_plugins[i] != nullptr; ++i)
		if (decoder_plugins_enabled[i])
			f(*decoder_plugins[i]);
}

/**
 * Is there at least once #DecoderPlugin that supports the specified
 * file name suffix?
 */
[[gnu::pure]]
bool
decoder_plugins_supports_suffix(std::string_view suffix) noexcept;

#endif
