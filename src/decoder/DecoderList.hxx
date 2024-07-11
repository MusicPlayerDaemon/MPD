// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "util/DereferenceIterator.hxx"
#include "util/FilteredContainer.hxx"
#include "util/TerminatedArray.hxx"

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

static inline auto
GetAllDecoderPlugins() noexcept
{
	return DereferenceContainerAdapter{TerminatedArray<const DecoderPlugin *const, nullptr>{decoder_plugins}};
}

static inline auto
GetEnabledDecoderPlugins() noexcept
{
	const auto all = GetAllDecoderPlugins();
	return FilteredContainer{all.begin(), all.end(), decoder_plugins_enabled};
}

template<typename F>
static inline const DecoderPlugin *
decoder_plugins_find(F f) noexcept
{
	for (const auto &plugin : GetEnabledDecoderPlugins())
		if (f(plugin))
			return &plugin;

	return nullptr;
}

/**
 * Is there at least once #DecoderPlugin that supports the specified
 * file name suffix?
 */
[[gnu::pure]]
bool
decoder_plugins_supports_suffix(std::string_view suffix) noexcept;
