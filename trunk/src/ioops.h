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

#ifndef IOOPS_H
#define IOOPS_H

#include <sys/select.h>

struct ioOps {
	struct ioOps *prev, *next;

	/*
	 * Called before each 'select' statement.
	 *   To register for IO, call FD_SET for each required queue
	 * Return the highest fd number you registered
	 */
	int (*fdset) ( fd_set *rfds, fd_set *wfds, fd_set *efds );

	/*
	 * Called after each 'select' statement.
	 *   fdCount is the number of fds total in all sets.  It may be 0.
	 *   For each fd you registered for in (fdset), you should FD_CLR it from the
	 *   appropriate queue(s).
	 * Return the total number of fds left in all sets (Ie, return fdCount
	 *   minus the number of times you called FD_CLR).
	 */
	int (*consume) ( int fdCount, fd_set *rfds, fd_set *wfds, fd_set *efds );
};

/* Call this to register your io operation handler struct */
void registerIO( struct ioOps *ops );

/* Call this to deregister your io operation handler struct */
void deregisterIO( struct ioOps *ops );

#endif
