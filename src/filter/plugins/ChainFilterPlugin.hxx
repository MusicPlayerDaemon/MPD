/*
 * Copyright 2003-2019 The Music Player Daemon Project
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
 * A filter chain is a container for several filters.  They are
 * chained together, i.e. called in a row, one filter passing its
 * output to the next one.
 */

#ifndef MPD_FILTER_CHAIN_HXX
#define MPD_FILTER_CHAIN_HXX

#include <memory>

class PreparedFilter;

/**
 * Creates a new filter chain.
 */
std::unique_ptr<PreparedFilter>
filter_chain_new() noexcept;

/**
 * Appends a new filter at the end of the filter chain.  You must call
 * this function before the first filter_open() call.
 *
 * @param chain the filter chain created with filter_chain_new()
 * @param filter the filter to be appended to #chain
 */
void
filter_chain_append(PreparedFilter &chain, const char *name,
		    std::unique_ptr<PreparedFilter> filter) noexcept;

#endif
