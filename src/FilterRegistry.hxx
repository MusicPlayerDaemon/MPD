/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
 * This library manages all filter plugins which are enabled at
 * compile time.
 */

#ifndef MPD_FILTER_REGISTRY_HXX
#define MPD_FILTER_REGISTRY_HXX

#include "Compiler.h"

extern const struct filter_plugin null_filter_plugin;
extern const struct filter_plugin chain_filter_plugin;
extern const struct filter_plugin convert_filter_plugin;
extern const struct filter_plugin route_filter_plugin;
extern const struct filter_plugin normalize_filter_plugin;
extern const struct filter_plugin volume_filter_plugin;
extern const struct filter_plugin replay_gain_filter_plugin;

gcc_pure
const struct filter_plugin *
filter_plugin_by_name(const char *name);

#endif
