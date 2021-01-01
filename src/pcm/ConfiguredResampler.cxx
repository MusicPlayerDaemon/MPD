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

#include "ConfiguredResampler.hxx"
#include "FallbackResampler.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "config/Block.hxx"
#include "config/Param.hxx"
#include "util/RuntimeError.hxx"
#include "config.h"

#ifdef ENABLE_LIBSAMPLERATE
#include "LibsamplerateResampler.hxx"
#endif

#ifdef ENABLE_SOXR
#include "SoxrResampler.hxx"
#endif

#include <cassert>

#include <string.h>

enum class SelectedResampler {
	FALLBACK,

#ifdef ENABLE_LIBSAMPLERATE
	LIBSAMPLERATE,
#endif

#ifdef ENABLE_SOXR
	SOXR,
#endif
};

static SelectedResampler selected_resampler = SelectedResampler::FALLBACK;

static const ConfigBlock *
MakeResamplerDefaultConfig(ConfigBlock &block) noexcept
{
	assert(block.IsEmpty());

#ifdef ENABLE_LIBSAMPLERATE
	block.AddBlockParam("plugin", "libsamplerate");
#elif defined(ENABLE_SOXR)
	block.AddBlockParam("plugin", "soxr");
#else
	block.AddBlockParam("plugin", "internal");
#endif
	return &block;
}

/**
 * Convert the old "samplerate_converter" setting to a new-style
 * "resampler" block.
 */
static const ConfigBlock *
MigrateResamplerConfig(const ConfigParam &param, ConfigBlock &block) noexcept
{
	assert(block.IsEmpty());

	block.line = param.line;

	const char *converter = param.value.c_str();
	if (*converter == 0 || strcmp(converter, "internal") == 0) {
		block.AddBlockParam("plugin", "internal");
		return &block;
	}

#ifdef ENABLE_SOXR
	if (strcmp(converter, "soxr") == 0) {
		block.AddBlockParam("plugin", "soxr");
		return &block;
	}

	if (memcmp(converter, "soxr ", 5) == 0) {
		block.AddBlockParam("plugin", "soxr");
		block.AddBlockParam("quality", converter + 5);
		return &block;
	}
#endif

	block.AddBlockParam("plugin", "libsamplerate");
	block.AddBlockParam("type", converter);
	return &block;
}

static const ConfigBlock *
MigrateResamplerConfig(const ConfigParam *param, ConfigBlock &buffer) noexcept
{
	assert(buffer.IsEmpty());

	return param == nullptr
		? MakeResamplerDefaultConfig(buffer)
		: MigrateResamplerConfig(*param, buffer);
}

static const ConfigBlock *
GetResamplerConfig(const ConfigData &config, ConfigBlock &buffer)
{
	const auto *old_param =
		config.GetParam(ConfigOption::SAMPLERATE_CONVERTER);
	const auto *block = config.GetBlock(ConfigBlockOption::RESAMPLER);
	if (block == nullptr)
		return MigrateResamplerConfig(old_param, buffer);

	if (old_param != nullptr)
		throw FormatRuntimeError("Cannot use both 'resampler' (line %d) and 'samplerate_converter' (line %d)",
					 block->line, old_param->line);

	block->SetUsed();
	return block;
}

void
pcm_resampler_global_init(const ConfigData &config)
{
	ConfigBlock buffer;
	const auto *block = GetResamplerConfig(config, buffer);

	const char *plugin_name = block->GetBlockValue("plugin");
	if (plugin_name == nullptr)
		throw FormatRuntimeError("'plugin' missing in line %d",
					 block->line);

	if (strcmp(plugin_name, "internal") == 0) {
		selected_resampler = SelectedResampler::FALLBACK;
#ifdef ENABLE_SOXR
	} else if (strcmp(plugin_name, "soxr") == 0) {
		selected_resampler = SelectedResampler::SOXR;
		pcm_resample_soxr_global_init(*block);
#endif
#ifdef ENABLE_LIBSAMPLERATE
	} else if (strcmp(plugin_name, "libsamplerate") == 0) {
		selected_resampler = SelectedResampler::LIBSAMPLERATE;
		pcm_resample_lsr_global_init(*block);
#endif
	} else {
		throw FormatRuntimeError("No such resampler plugin: %s",
					 plugin_name);
	}
}

PcmResampler *
pcm_resampler_create()
{
	switch (selected_resampler) {
	case SelectedResampler::FALLBACK:
		return new FallbackPcmResampler();

#ifdef ENABLE_LIBSAMPLERATE
	case SelectedResampler::LIBSAMPLERATE:
		return new LibsampleratePcmResampler();
#endif

#ifdef ENABLE_SOXR
	case SelectedResampler::SOXR:
		return new SoxrPcmResampler();
#endif
	}

	gcc_unreachable();
}
