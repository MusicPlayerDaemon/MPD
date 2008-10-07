#ifndef SONGVEC_H
#define SONGVEC_H

#include "song.h"
#include "os_compat.h"

struct songvec {
	Song **base;
	size_t nr;
};

void songvec_sort(struct songvec *sv);

Song *
songvec_find(const struct songvec *sv, const char *url);

int songvec_delete(struct songvec *sv, const Song *del);

void songvec_add(struct songvec *sv, Song *add);

void songvec_destroy(struct songvec *sv);

int
songvec_for_each(const struct songvec *sv,
		 int (*fn)(Song *, void *), void *arg);

#endif /* SONGVEC_H */
