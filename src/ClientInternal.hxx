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

#ifndef MPD_CLIENT_INTERNAL_HXX
#define MPD_CLIENT_INTERNAL_HXX

#include "check.h"
#include "Client.hxx"
#include "command/CommandResult.hxx"

static constexpr unsigned CLIENT_MAX_SUBSCRIPTIONS = 16;
static constexpr unsigned CLIENT_MAX_MESSAGES = 64;

extern const class Domain client_domain;

extern int client_timeout;
extern size_t client_max_command_list_size;
extern size_t client_max_output_buffer_size;

CommandResult
client_process_line(Client &client, char *line);

#endif
