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

#ifndef MPD_QUEUE_COMMANDS_HXX
#define MPD_QUEUE_COMMANDS_HXX

#include "command.h"

enum command_return
handle_add(struct client *client, int argc, char *argv[]);

enum command_return
handle_addid(struct client *client, int argc, char *argv[]);

enum command_return
handle_delete(struct client *client, int argc, char *argv[]);

enum command_return
handle_deleteid(struct client *client, int argc, char *argv[]);

enum command_return
handle_playlist(struct client *client, int argc, char *argv[]);

enum command_return
handle_shuffle(struct client *client, int argc, char *argv[]);

enum command_return
handle_clear(struct client *client, int argc, char *argv[]);

enum command_return
handle_plchanges(struct client *client, int argc, char *argv[]);

enum command_return
handle_plchangesposid(struct client *client, int argc, char *argv[]);

enum command_return
handle_playlistinfo(struct client *client, int argc, char *argv[]);

enum command_return
handle_playlistid(struct client *client, int argc, char *argv[]);

enum command_return
handle_playlistfind(struct client *client, int argc, char *argv[]);

enum command_return
handle_playlistsearch(struct client *client, int argc, char *argv[]);

enum command_return
handle_prio(struct client *client, int argc, char *argv[]);

enum command_return
handle_prioid(struct client *client, int argc, char *argv[]);

enum command_return
handle_move(struct client *client, int argc, char *argv[]);

enum command_return
handle_moveid(struct client *client, int argc, char *argv[]);

enum command_return
handle_swap(struct client *client, int argc, char *argv[]);

enum command_return
handle_swapid(struct client *client, int argc, char *argv[]);

#endif
