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

#ifndef MPD_PLAYER_COMMANDS_HXX
#define MPD_PLAYER_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;
template<typename T> struct ConstBuffer;

CommandResult
handle_play(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_playid(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_stop(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_currentsong(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_pause(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_status(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_next(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_previous(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_repeat(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_single(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_consume(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_random(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_clearerror(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_seek(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_seekid(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_seekcur(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_crossfade(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_mixrampdb(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_mixrampdelay(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_replay_gain_mode(Client &client, ConstBuffer<const char *> args);

CommandResult
handle_replay_gain_status(Client &client, ConstBuffer<const char *> args);

#endif
