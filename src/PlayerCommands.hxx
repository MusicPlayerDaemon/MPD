/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

#include "command.h"

enum command_return
handle_play(struct client *client, int argc, char *argv[]);

enum command_return
handle_playid(struct client *client, int argc, char *argv[]);

enum command_return
handle_stop(struct client *client, int argc, char *argv[]);

enum command_return
handle_currentsong(struct client *client, int argc, char *argv[]);

enum command_return
handle_pause(struct client *client, int argc, char *argv[]);

enum command_return
handle_status(struct client *client, int argc, char *argv[]);

enum command_return
handle_next(struct client *client, int argc, char *argv[]);

enum command_return
handle_previous(struct client *client, int argc, char *avg[]);

enum command_return
handle_repeat(struct client *client, int argc, char *argv[]);

enum command_return
handle_single(struct client *client, int argc, char *argv[]);

enum command_return
handle_consume(struct client *client, int argc, char *argv[]);

enum command_return
handle_random(struct client *client, int argc, char *argv[]);

enum command_return
handle_clearerror(struct client *client, int argc, char *argv[]);

enum command_return
handle_seek(struct client *client, int argc, char *argv[]);

enum command_return
handle_seekid(struct client *client, int argc, char *argv[]);

enum command_return
handle_seekcur(struct client *client, int argc, char *argv[]);

enum command_return
handle_crossfade(struct client *client, int argc, char *argv[]);

enum command_return
handle_mixrampdb(struct client *client, int argc, char *argv[]);

enum command_return
handle_mixrampdelay(struct client *client, int argc, char *argv[]);

enum command_return
handle_replay_gain_mode(struct client *client, int argc, char *argv[]);

enum command_return
handle_replay_gain_status(struct client *client, int argc, char *argv[]);

#endif
