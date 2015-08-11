/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#ifndef MPD_OTHER_COMMANDS_HXX
#define MPD_OTHER_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
class Request;

CommandResult
handle_urlhandlers(Client &client, Request args);

CommandResult
handle_decoders(Client &client, Request args);

CommandResult
handle_tagtypes(Client &client, Request args);

CommandResult
handle_kill(Client &client, Request args);

CommandResult
handle_close(Client &client, Request args);

CommandResult
handle_listfiles(Client &client, Request args);

CommandResult
handle_lsinfo(Client &client, Request args);

CommandResult
handle_update(Client &client, Request args);

CommandResult
handle_rescan(Client &client, Request args);

CommandResult
handle_setvol(Client &client, Request args);

CommandResult
handle_volume(Client &client, Request args);

CommandResult
handle_stats(Client &client, Request args);

CommandResult
handle_ping(Client &client, Request args);

CommandResult
handle_password(Client &client, Request args);

CommandResult
handle_config(Client &client, Request args);

CommandResult
handle_idle(Client &client, Request args);

#endif
