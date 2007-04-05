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

/* a very simple singly-linked-list structure for queues/buffers */

#ifndef SLLIST_H
#define SLLIST_H

#include <stddef.h>

/* just free the entire structure if it's free-able, the 'data' member
 * should _NEVER_ be explicitly freed
 *
 * there's no free command, iterate through them yourself and just
 * call free() on it iff you xmalloc'd them */

struct strnode {
	struct strnode *next;
	char *data;
};

struct sllnode {
	struct sllnode *next;
	void *data;
	size_t size;
};

struct strnode *new_strnode(char *s);

struct strnode *new_strnode_dup(char *s, const size_t size);

struct strnode *dup_strlist(struct strnode *old);

struct sllnode *new_sllnode(void *s, const size_t size);


#endif /* SLLIST_H */
