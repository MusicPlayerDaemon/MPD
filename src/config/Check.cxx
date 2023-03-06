// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Check.hxx"
#include "Data.hxx"
#include "Domain.hxx"
#include "Log.hxx"

static void
Check(const ConfigBlock &block)
{
	if (!block.used)
		/* this whole block was not queried at all -
		   the feature might be disabled at compile time?
		   Silently ignore it here. */
		return;

	for (const auto &i : block.block_params) {
		if (!i.used)
			FmtWarning(config_domain,
				   "option '{}' on line {} was not recognized",
				   i.name, i.line);
	}
}

void
Check(const ConfigData &config_data) noexcept
{
	for (const auto &list : config_data.blocks)
		for (const auto &block : list)
			Check(block);
}
