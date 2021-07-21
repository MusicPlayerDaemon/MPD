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

/** \file
 *
 * This filter plugin does nothing.  That is not quite useful, except
 * for testing the filter core, or as a template for new filter
 * plugins.
 */

#include "NullFilterPlugin.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/NullFilter.hxx"
#include "filter/Prepared.hxx"
#include "util/Compiler.h"

class PreparedNullFilter final : public PreparedFilter {
public:
	std::unique_ptr<Filter> Open(AudioFormat &af) override {
		return std::make_unique<NullFilter>(af);
	}
};

static std::unique_ptr<PreparedFilter>
null_filter_init([[maybe_unused]] const ConfigBlock &block)
{
	return std::make_unique<PreparedNullFilter>();
}

const FilterPlugin null_filter_plugin = {
	"null",
	null_filter_init,
};
