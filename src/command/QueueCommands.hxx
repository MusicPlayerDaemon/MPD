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

#ifndef MPD_QUEUE_COMMANDS_HXX
#define MPD_QUEUE_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
template<typename T> struct ConstBuffer;

CommandResult
handle_add(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_addid(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_rangeid(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_delete(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_deleteid(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_playlist(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_shuffle(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_clear(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_plchanges(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_plchangesposid(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_playlistinfo(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_playlistid(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_playlistfind(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_playlistsearch(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_prio(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_prioid(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_move(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_moveid(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_swap(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_swapid(Client &client, ConstBuffer<const char *> args);

#endif
