/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include "config.h"
#include "filter/FilterPlugin.hxx"
#include "filter/FilterInternal.hxx"
#include "filter/FilterRegistry.hxx"
#include "AudioFormat.hxx"
#include "Compiler.h"
#include "util/ConstBuffer.hxx"

class NullFilter final : public Filter {
public:
	explicit NullFilter(const AudioFormat &af):Filter(af) {}

	virtual ConstBuffer<void> FilterPCM(ConstBuffer<void> src,
					    gcc_unused Error &error) override {
		return src;
	}
};

class PreparedNullFilter final : public PreparedFilter {
public:
	virtual Filter *Open(AudioFormat &af,
			     gcc_unused Error &error) override {
		return new NullFilter(af);
	}
};

static PreparedFilter *
null_filter_init(gcc_unused const ConfigBlock &block,
		 gcc_unused Error &error)
{
	return new PreparedNullFilter();
}

const struct filter_plugin null_filter_plugin = {
	"null",
	null_filter_init,
};
