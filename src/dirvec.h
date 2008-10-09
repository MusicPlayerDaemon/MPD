#ifndef DIRVEC_H
#define DIRVEC_H

#include <stddef.h>

struct dirvec {
	struct directory **base;
	size_t nr;
};

void dirvec_sort(struct dirvec *dv);

struct directory *dirvec_find(struct dirvec *dv, const char *path);

int dirvec_delete(struct dirvec *dv, struct directory *del);

void dirvec_add(struct dirvec *dv, struct directory *add);

void dirvec_destroy(struct dirvec *dv);

#endif /* DIRVEC_H */
