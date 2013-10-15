/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "FilterPlugin.hxx"
#include "FilterInternal.hxx"
#include "FilterRegistry.hxx"
#include "AudioFormat.hxx"
#include "Compiler.h"

class NullFilter final : public Filter {
public:
	virtual AudioFormat Open(AudioFormat &af,
				 gcc_unused Error &error) override {
		return af;
	}

	virtual void Close() override {}

	virtual const void *FilterPCM(const void *src, size_t src_size,
				      size_t *dest_size_r,
				      gcc_unused Error &error) override {
		*dest_size_r = src_size;
		return src;
	}
};

static Filter *
null_filter_init(gcc_unused const config_param &param,
		 gcc_unused Error &error)
{
	return new NullFilter();
}

const struct filter_plugin null_filter_plugin = {
	"null",
	null_filter_init,
};
