/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef LISTEN_H
#define LISTEN_H

#include "../config.h"

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>

void listenOnPort(void);

void getConnections(fd_set * fds);

void closeAllListenSockets(void);
void freeAllListenSockets(void);

/* fdmax should be initialized to something */
void addListenSocketsToFdSet(fd_set * fds, int *fdmax);

int getBoundPort(void);

#endif
