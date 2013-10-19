/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#ifndef MPD_CLIENT_SUBSCRIBE_HXX
#define MPD_CLIENT_SUBSCRIBE_HXX

#include "Compiler.h"

class Client;
class ClientMessage;

enum client_subscribe_result {
	/** success */
	CLIENT_SUBSCRIBE_OK,

	/** invalid channel name */
	CLIENT_SUBSCRIBE_INVALID,

	/** already subscribed to this channel */
	CLIENT_SUBSCRIBE_ALREADY,

	/** too many subscriptions */
	CLIENT_SUBSCRIBE_FULL,
};

enum client_subscribe_result
client_subscribe(Client &client, const char *channel);

bool
client_unsubscribe(Client &client, const char *channel);

void
client_unsubscribe_all(Client &client);

bool
client_push_message(Client &client, const ClientMessage &msg);

#endif
