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
