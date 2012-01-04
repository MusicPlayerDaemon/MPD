/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
 * http://www.musicpd.org
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "mixer_api.h"
#include "output_api.h"
#include "event_pipe.h"

#include <glib.h>
#include <alsa/asoundlib.h>

#define VOLUME_MIXER_ALSA_DEFAULT		"default"
#define VOLUME_MIXER_ALSA_CONTROL_DEFAULT	"PCM"
#define VOLUME_MIXER_ALSA_INDEX_DEFAULT		0

struct alsa_mixer_source {
	GSource source;

	snd_mixer_t *mixer;

	/** a linked list of all registered GPollFD objects */
	GSList *fds;
};

struct alsa_mixer {
	/** the base mixer class */
	struct mixer base;

	const char *device;
	const char *control;
	unsigned int index;

	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	long volume_min;
	long volume_max;
	int volume_set;

	struct alsa_mixer_source *source;
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
alsa_mixer_quark(void)
{
	return g_quark_from_static_string("alsa_mixer");
}

/*
 * GSource helper functions
 *
 */

static GSList **
find_fd(GSList **list_r, int fd)
{
	while (true) {
		GSList *list = *list_r;
		if (list == NULL)
			return NULL;

		GPollFD *p = list->data;
		if (p->fd == fd)
			return list_r;

		list_r = &list->next;
	}
}

static void
alsa_mixer_update_fd(struct alsa_mixer_source *source, const struct pollfd *p,
		     GSList **old_r)
{
	GSList **found_r = find_fd(old_r, p->fd);
	if (found_r == NULL) {
		/* new fd */
		GPollFD *q = g_new(GPollFD, 1);
		q->fd = p->fd;
		q->events = p->events;
		g_source_add_poll(&source->source, q);
		source->fds = g_slist_prepend(source->fds, q);
		return;
	}

	GSList *found = *found_r;
	*found_r = found->next;

	GPollFD *q = found->data;
	if (q->events != p->events) {
		/* refresh events */
		g_source_remove_poll(&source->source, q);
		q->events = p->events;
		g_source_add_poll(&source->source, q);
	}

	found->next = source->fds;
	source->fds = found;
}

static void
alsa_mixer_update_fds(struct alsa_mixer_source *source)
{
	int count = snd_mixer_poll_descriptors_count(source->mixer);
	if (count < 0)
		count = 0;

	struct pollfd *pfds = g_new(struct pollfd, count);
	count = snd_mixer_poll_descriptors(source->mixer, pfds, count);
	if (count < 0)
		count = 0;

	GSList *old = source->fds;
	source->fds = NULL;

	for (int i = 0; i < count; ++i)
		alsa_mixer_update_fd(source, &pfds[i], &old);
	g_free(pfds);

	for (; old != NULL; old = old->next) {
		GPollFD *q = old->data;
		g_source_remove_poll(&source->source, q);
		g_free(q);
	}

	g_slist_free(old);
}

/*
 * GSource methods
 *
 */

static gboolean
alsa_mixer_source_prepare(GSource *_source, G_GNUC_UNUSED gint *timeout_r)
{
	struct alsa_mixer_source *source = (struct alsa_mixer_source *)_source;
	alsa_mixer_update_fds(source);

	return false;
}

static gboolean
alsa_mixer_source_check(GSource *_source)
{
	struct alsa_mixer_source *source = (struct alsa_mixer_source *)_source;

	for (const GSList *i = source->fds; i != NULL; i = i->next) {
		const GPollFD *poll_fd = i->data;
		if (poll_fd->revents != 0)
			return true;
	}

	return false;
}

static gboolean
alsa_mixer_source_dispatch(GSource *_source,
			   G_GNUC_UNUSED GSourceFunc callback,
			   G_GNUC_UNUSED gpointer user_data)
{
	struct alsa_mixer_source *source = (struct alsa_mixer_source *)_source;

	snd_mixer_handle_events(source->mixer);
	return true;
}

static void
alsa_mixer_source_finalize(GSource *_source)
{
	struct alsa_mixer_source *source = (struct alsa_mixer_source *)_source;

	for (GSList *i = source->fds; i != NULL; i = i->next)
		g_free(i->data);

	g_slist_free(source->fds);
}

static GSourceFuncs alsa_mixer_source_funcs = {
	.prepare = alsa_mixer_source_prepare,
	.check = alsa_mixer_source_check,
	.dispatch = alsa_mixer_source_dispatch,
	.finalize = alsa_mixer_source_finalize,
};

/*
 * libasound callbacks
 *
 */

static int
alsa_mixer_elem_callback(G_GNUC_UNUSED snd_mixer_elem_t *elem, unsigned mask)
{
	if (mask & SND_CTL_EVENT_MASK_VALUE)
		event_pipe_emit(PIPE_EVENT_MIXER);

	return 0;
}

/*
 * mixer_plugin methods
 *
 */

static struct mixer *
alsa_mixer_init(G_GNUC_UNUSED void *ao, const struct config_param *param,
		G_GNUC_UNUSED GError **error_r)
{
	struct alsa_mixer *am = g_new(struct alsa_mixer, 1);

	mixer_init(&am->base, &alsa_mixer_plugin);

	am->device = config_get_block_string(param, "mixer_device",
					     VOLUME_MIXER_ALSA_DEFAULT);
	am->control = config_get_block_string(param, "mixer_control",
					      VOLUME_MIXER_ALSA_CONTROL_DEFAULT);
	am->index = config_get_block_unsigned(param, "mixer_index",
					      VOLUME_MIXER_ALSA_INDEX_DEFAULT);

	return &am->base;
}

static void
alsa_mixer_finish(struct mixer *data)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;

	g_free(am);

	/* free libasound's config cache */
	snd_config_update_free_global();
}

G_GNUC_PURE
static snd_mixer_elem_t *
alsa_mixer_lookup_elem(snd_mixer_t *handle, const char *name, unsigned idx)
{
	for (snd_mixer_elem_t *elem = snd_mixer_first_elem(handle);
	     elem != NULL; elem = snd_mixer_elem_next(elem)) {
		if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE &&
		    g_ascii_strcasecmp(snd_mixer_selem_get_name(elem),
				       name) == 0 &&
		    snd_mixer_selem_get_index(elem) == idx)
			return elem;
	}

	return NULL;
}

static bool
alsa_mixer_setup(struct alsa_mixer *am, GError **error_r)
{
	int err;

	if ((err = snd_mixer_attach(am->handle, am->device)) < 0) {
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "failed to attach to %s: %s",
			    am->device, snd_strerror(err));
		return false;
	}

	if ((err = snd_mixer_selem_register(am->handle, NULL,
		    NULL)) < 0) {
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "snd_mixer_selem_register() failed: %s",
			    snd_strerror(err));
		return false;
	}

	if ((err = snd_mixer_load(am->handle)) < 0) {
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "snd_mixer_load() failed: %s\n",
			    snd_strerror(err));
		return false;
	}

	am->elem = alsa_mixer_lookup_elem(am->handle, am->control, am->index);
	if (am->elem == NULL) {
		g_set_error(error_r, alsa_mixer_quark(), 0,
			    "no such mixer control: %s", am->control);
		return false;
	}

	snd_mixer_selem_get_playback_volume_range(am->elem,
						  &am->volume_min,
						  &am->volume_max);

	snd_mixer_elem_set_callback(am->elem, alsa_mixer_elem_callback);

	am->source = (struct alsa_mixer_source *)
		g_source_new(&alsa_mixer_source_funcs, sizeof(*am->source));
	am->source->mixer = am->handle;
	am->source->fds = NULL;
	g_source_attach(&am->source->source, g_main_context_default());

	return true;
}

static bool
alsa_mixer_open(struct mixer *data, GError **error_r)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;
	int err;

	am->volume_set = -1;

	err = snd_mixer_open(&am->handle, 0);
	if (err < 0) {
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "snd_mixer_open() failed: %s", snd_strerror(err));
		return false;
	}

	if (!alsa_mixer_setup(am, error_r)) {
		snd_mixer_close(am->handle);
		return false;
	}

	return true;
}

static void
alsa_mixer_close(struct mixer *data)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;

	assert(am->handle != NULL);

	g_source_destroy(&am->source->source);
	g_source_unref(&am->source->source);

	snd_mixer_elem_set_callback(am->elem, NULL);
	snd_mixer_close(am->handle);
}

static int
alsa_mixer_get_volume(struct mixer *mixer, GError **error_r)
{
	struct alsa_mixer *am = (struct alsa_mixer *)mixer;
	int err;
	int ret;
	long level;

	assert(am->handle != NULL);

	err = snd_mixer_handle_events(am->handle);
	if (err < 0) {
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "snd_mixer_handle_events() failed: %s",
			    snd_strerror(err));
		return false;
	}

	err = snd_mixer_selem_get_playback_volume(am->elem,
						  SND_MIXER_SCHN_FRONT_LEFT,
						  &level);
	if (err < 0) {
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "failed to read ALSA volume: %s",
			    snd_strerror(err));
		return false;
	}

	ret = ((am->volume_set / 100.0) * (am->volume_max - am->volume_min)
	       + am->volume_min) + 0.5;
	if (am->volume_set > 0 && ret == level) {
		ret = am->volume_set;
	} else {
		ret = (int)(100 * (((float)(level - am->volume_min)) /
				   (am->volume_max - am->volume_min)) + 0.5);
	}

	return ret;
}

static bool
alsa_mixer_set_volume(struct mixer *mixer, unsigned volume, GError **error_r)
{
	struct alsa_mixer *am = (struct alsa_mixer *)mixer;
	float vol;
	long level;
	int err;

	assert(am->handle != NULL);

	vol = volume;

	am->volume_set = vol + 0.5;

	level = (long)(((vol / 100.0) * (am->volume_max - am->volume_min) +
			am->volume_min) + 0.5);
	level = level > am->volume_max ? am->volume_max : level;
	level = level < am->volume_min ? am->volume_min : level;

	err = snd_mixer_selem_set_playback_volume_all(am->elem, level);
	if (err < 0) {
		g_set_error(error_r, alsa_mixer_quark(), err,
			    "failed to set ALSA volume: %s",
			    snd_strerror(err));
		return false;
	}

	return true;
}

const struct mixer_plugin alsa_mixer_plugin = {
	.init = alsa_mixer_init,
	.finish = alsa_mixer_finish,
	.open = alsa_mixer_open,
	.close = alsa_mixer_close,
	.get_volume = alsa_mixer_get_volume,
	.set_volume = alsa_mixer_set_volume,
	.global = true,
};
