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

#include "ioops.h"

/* Eventually the listener protocol will use this, too */

#ifdef HAVE_ZEROCONF

/*
 * functions and variables in this file are only used by a single thread and
 * thus do not need to be thread-safe
 */

/* List of registered external IO handlers */
static struct ioOps *ioList;

/* Add fds for all registered IO handlers */
void registered_IO_add_fds(int *fdmax,
                           fd_set * rfds, fd_set * wfds, fd_set * efds)
{
	struct ioOps *o = ioList;

	while (o) {
		struct ioOps *current = o;
		int fdnum;

		assert(current->fdset);
		fdnum = current->fdset(rfds, wfds, efds);
		if (*fdmax < fdnum)
			*fdmax = fdnum;
		o = o->next;
	}
}

/* Consume fds for all registered IO handlers */
void registered_IO_consume_fds(int *selret,
			       fd_set * rfds, fd_set * wfds, fd_set * efds)
{
	struct ioOps *o = ioList;

	while (o) {
		struct ioOps *current = o;

		assert(current->consume);
		*selret = current->consume(*selret, rfds, wfds, efds);
		o = o->next;
	}
}

void registerIO(struct ioOps *ops)
{
	assert(ops != NULL);

	ops->next = ioList;
	ioList = ops;
	ops->prev = NULL;
	if (ops->next)
		ops->next->prev = ops;
}

void deregisterIO(struct ioOps *ops)
{
	assert(ops != NULL);

	if (ioList == ops)
		ioList = ops->next;
	else if (ops->prev != NULL)
		ops->prev->next = ops->next;
}

#endif /* HAVE_ZEROCONF */
