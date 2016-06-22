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
 * Utility functions for filter configuration
 */

#ifndef MPD_FILTER_CONFIG_HXX
#define MPD_FILTER_CONFIG_HXX

class PreparedFilter;
class Error;

/**
 * Builds a filter chain from a configuration string on the form
 * "name1, name2, name3, ..." by looking up each name among the
 * configured filter sections.
 * @param chain the chain to append filters on
 * @param spec the filter chain specification
 * @param error space to return an error description
 * @return true on success
 */
bool
filter_chain_parse(PreparedFilter &chain, const char *spec, Error &error);

#endif
