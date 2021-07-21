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

#ifndef MPD_AUTOCONVERT_FILTER_PLUGIN_HXX
#define MPD_AUTOCONVERT_FILTER_PLUGIN_HXX

#include <memory>

class PreparedFilter;

/**
 * Creates a new "autoconvert" filter.  When opened, it ensures that
 * the input audio format isn't changed.  If the underlying filter
 * requests a different format, it automatically creates a
 * convert_filter.
 */
std::unique_ptr<PreparedFilter>
autoconvert_filter_new(std::unique_ptr<PreparedFilter> filter) noexcept;

#endif
