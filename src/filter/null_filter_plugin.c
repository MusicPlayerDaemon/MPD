/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

/** \file
 *
 * This filter plugin does nothing.  That is not quite useful, except
 * for testing the filter core, or as a template for new filter
 * plugins.
 */

#include "config.h"
#include "filter_plugin.h"
#include "filter_internal.h"
#include "filter_registry.h"

#include <assert.h>

struct null_filter {
	struct filter filter;
};

static struct filter *
null_filter_init(G_GNUC_UNUSED const struct config_param *param,
		 G_GNUC_UNUSED GError **error_r)
{
	struct null_filter *filter = g_new(struct null_filter, 1);

	filter_init(&filter->filter, &null_filter_plugin);
	return &filter->filter;
}

static void
null_filter_finish(struct filter *_filter)
{
	struct null_filter *filter = (struct null_filter *)_filter;

	g_free(filter);
}

static const struct audio_format *
null_filter_open(struct filter *_filter,
		 const struct audio_format *audio_format,
		 G_GNUC_UNUSED GError **error_r)
{
	struct null_filter *filter = (struct null_filter *)_filter;
	(void)filter;

	return audio_format;
}

static void
null_filter_close(struct filter *_filter)
{
	struct null_filter *filter = (struct null_filter *)_filter;
	(void)filter;
}

static const void *
null_filter_filter(struct filter *_filter,
		   const void *src, size_t src_size,
		   size_t *dest_size_r, G_GNUC_UNUSED GError **error_r)
{
	struct null_filter *filter = (struct null_filter *)_filter;
	(void)filter;

	/* return the unmodified source buffer */
	*dest_size_r = src_size;
	return src;
}

const struct filter_plugin null_filter_plugin = {
	.name = "null",
	.init = null_filter_init,
	.finish = null_filter_finish,
	.open = null_filter_open,
	.close = null_filter_close,
	.filter = null_filter_filter,
};
