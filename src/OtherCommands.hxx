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

#ifndef MPD_OTHER_COMMANDS_HXX
#define MPD_OTHER_COMMANDS_HXX

#include "command.h"

class Client;

enum command_return
handle_urlhandlers(Client *client, int argc, char *argv[]);

enum command_return
handle_decoders(Client *client, int argc, char *argv[]);

enum command_return
handle_tagtypes(Client *client, int argc, char *argv[]);

enum command_return
handle_kill(Client *client, int argc, char *argv[]);

enum command_return
handle_close(Client *client, int argc, char *argv[]);

enum command_return
handle_lsinfo(Client *client, int argc, char *argv[]);

enum command_return
handle_update(Client *client, int argc, char *argv[]);

enum command_return
handle_rescan(Client *client, int argc, char *argv[]);

enum command_return
handle_setvol(Client *client, int argc, char *argv[]);

enum command_return
handle_stats(Client *client, int argc, char *argv[]);

enum command_return
handle_ping(Client *client, int argc, char *argv[]);

enum command_return
handle_password(Client *client, int argc, char *argv[]);

enum command_return
handle_config(Client *client, int argc, char *argv[]);

enum command_return
handle_idle(Client *client, int argc, char *argv[]);

#endif
