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

#include "Factory.hxx"
#include "LoadOne.hxx"
#include "Prepared.hxx"
#include "config/Data.hxx"
#include "config/Block.hxx"
#include "util/RuntimeError.hxx"

std::unique_ptr<PreparedFilter>
FilterFactory::MakeFilter(const char *name)
{
	const auto *cfg = config.FindBlock(ConfigBlockOption::AUDIO_FILTER,
					   "name", name);
	if (cfg == nullptr)
		throw FormatRuntimeError("Filter template not found: %s",
					 name);

	cfg->SetUsed();

	return filter_configured_new(*cfg);
}
