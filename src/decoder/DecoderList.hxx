// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
