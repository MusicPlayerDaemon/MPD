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

#include <string.h>
#include "sllist.h"
#include "utils.h"

static void init_strnode(struct strnode *x, char *s)
{
	x->data = s;
	x->next = NULL;
}

struct strnode *new_strnode(char *s)
{
	struct strnode *x = xmalloc(sizeof(struct strnode));
	init_strnode(x, s);
	return x;
}

struct strnode *new_strnode_dup(char *s, const size_t size)
{
	struct strnode *x = xmalloc(sizeof(struct strnode) + size);
	x->next = NULL;
	x->data = ((char *)x + sizeof(struct strnode));
	memcpy((void *)x->data, (void*)s, size);
	return x;
}

struct sllnode *new_sllnode(void *s, const size_t size)
{
	struct sllnode *x = xmalloc(sizeof(struct sllnode) + size);
	x->next = NULL;
	x->size = size;
	x->data = ((char *)x + sizeof(struct sllnode));
	memcpy(x->data, (void *)s, size);
	return x;
}

struct strnode *dup_strlist(struct strnode *old)
{
	struct strnode *tmp, *new, *cur;

	tmp = old;
	cur = new = new_strnode_dup(tmp->data, strlen(tmp->data) + 1);
	tmp = tmp->next;
	while (tmp) {
		cur->next = new_strnode_dup(tmp->data, strlen(tmp->data) + 1);
		cur = cur->next;
		tmp = tmp->next;
	}
	return new;
}


