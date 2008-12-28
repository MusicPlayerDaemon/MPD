#ifndef MPD_SONGVEC_H
#define MPD_SONGVEC_H

#include <stddef.h>

struct songvec {
	struct song **base;
	size_t nr;
};

void songvec_init(void);

void songvec_deinit(void);

void songvec_sort(struct songvec *sv);

struct song *
songvec_find(const struct songvec *sv, const char *url);

int
songvec_delete(struct songvec *sv, const struct song *del);

void
songvec_add(struct songvec *sv, struct song *add);

void songvec_destroy(struct songvec *sv);

int
songvec_for_each(const struct songvec *sv,
		 int (*fn)(struct song *, void *), void *arg);

#endif /* SONGVEC_H */
