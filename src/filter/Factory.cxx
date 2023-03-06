// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Factory.hxx"
#include "LoadOne.hxx"
#include "Prepared.hxx"
#include "config/Data.hxx"
#include "config/Block.hxx"
#include "lib/fmt/RuntimeError.hxx"

std::unique_ptr<PreparedFilter>
FilterFactory::MakeFilter(const char *name)
{
	const auto *cfg = config.FindBlock(ConfigBlockOption::AUDIO_FILTER,
					   "name", name);
	if (cfg == nullptr)
		throw FmtRuntimeError("Filter template not found: {}",
				      name);

	cfg->SetUsed();

	return filter_configured_new(*cfg);
}
