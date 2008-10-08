#ifndef DIRVEC_H
#define DIRVEC_H

#include "directory.h"

void dirvec_sort(struct dirvec *dv);

Directory *dirvec_find(struct dirvec *dv, const char *path);

int dirvec_delete(struct dirvec *dv, Directory *del);

void dirvec_add(struct dirvec *dv, Directory *add);

void dirvec_destroy(struct dirvec *dv);

#endif /* DIRVEC_H */
