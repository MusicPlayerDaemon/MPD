/* a very simple singly-linked-list structure for queues/buffers */

#ifndef SLLIST_H
#define SLLIST_H

#include <stddef.h>

/* just free the entire structure if it's free-able, the 'data' member
 * should _NEVER_ be explicitly freed
 *
 * there's no free command, iterate through them yourself and just
 * call free() on it iff you malloc'd them */

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
