#include "dirvec.h"
#include "directory.h"
#include "utils.h"
#include "path.h"

#include <string.h>
#include <glib.h>

static pthread_mutex_t nr_lock = PTHREAD_MUTEX_INITIALIZER;

static size_t dv_size(const struct dirvec *dv)
{
	return dv->nr * sizeof(struct directory *);
}

/* Only used for sorting/searching a dirvec, not general purpose compares */
static int dirvec_cmp(const void *d1, const void *d2)
{
	const struct directory *a = ((const struct directory * const *)d1)[0];
	const struct directory *b = ((const struct directory * const *)d2)[0];
	return strcmp(a->path, b->path);
}

void dirvec_sort(struct dirvec *dv)
{
	pthread_mutex_lock(&nr_lock);
	qsort(dv->base, dv->nr, sizeof(struct directory *), dirvec_cmp);
	pthread_mutex_unlock(&nr_lock);
}

struct directory *dirvec_find(const struct dirvec *dv, const char *path)
{
	char *basename;
	int i;
	struct directory *ret = NULL;

	basename = g_path_get_basename(path);

	pthread_mutex_lock(&nr_lock);
	for (i = dv->nr; --i >= 0; )
		if (!strcmp(directory_get_name(dv->base[i]), basename)) {
			ret = dv->base[i];
			break;
		}
	pthread_mutex_unlock(&nr_lock);

	g_free(basename);
	return ret;
}

int dirvec_delete(struct dirvec *dv, struct directory *del)
{
	size_t i;

	pthread_mutex_lock(&nr_lock);
	for (i = 0; i < dv->nr; ++i) {
		if (dv->base[i] != del)
			continue;
		/* we _don't_ call directory_free() here */
		if (!--dv->nr) {
			pthread_mutex_unlock(&nr_lock);
			free(dv->base);
			dv->base = NULL;
			return i;
		} else {
			memmove(&dv->base[i], &dv->base[i + 1],
				(dv->nr - i) * sizeof(struct directory *));
			dv->base = xrealloc(dv->base, dv_size(dv));
		}
		break;
	}
	pthread_mutex_unlock(&nr_lock);

	return i;
}

void dirvec_add(struct dirvec *dv, struct directory *add)
{
	pthread_mutex_lock(&nr_lock);
	++dv->nr;
	dv->base = xrealloc(dv->base, dv_size(dv));
	dv->base[dv->nr - 1] = add;
	pthread_mutex_unlock(&nr_lock);
}

void dirvec_destroy(struct dirvec *dv)
{
	pthread_mutex_lock(&nr_lock);
	dv->nr = 0;
	pthread_mutex_unlock(&nr_lock);
	if (dv->base) {
		free(dv->base);
		dv->base = NULL;
	}
}

int dirvec_for_each(const struct dirvec *dv,
                    int (*fn)(struct directory *, void *), void *arg)
{
	size_t i;

	pthread_mutex_lock(&nr_lock);
	for (i = 0; i < dv->nr; ++i) {
		struct directory *dir = dv->base[i];

		assert(dir);
		pthread_mutex_unlock(&nr_lock);
		if (fn(dir, arg) < 0)
			return -1;
		pthread_mutex_lock(&nr_lock); /* dv->nr may change in fn() */
	}
	pthread_mutex_unlock(&nr_lock);

	return 0;
}
