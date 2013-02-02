/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "ChainFilterPlugin.hxx"
#include "conf.h"
#include "FilterPlugin.hxx"
#include "FilterInternal.hxx"
#include "FilterRegistry.hxx"
#include "audio_format.h"

#include <glib.h>

#include <list>

#include <assert.h>

struct ChainFilter {
	/** the base class */
	struct filter base;

	std::list<struct filter *> children;

	ChainFilter() {
		filter_init(&base, &chain_filter_plugin);
	}

	~ChainFilter() {
		for (auto i : children)
			filter_free(i);
	}
};

static inline GQuark
filter_quark(void)
{
	return g_quark_from_static_string("filter");
}

static struct filter *
chain_filter_init(gcc_unused const struct config_param *param,
		  gcc_unused GError **error_r)
{
	ChainFilter *chain = new ChainFilter();

	return &chain->base;
}

static void
chain_filter_finish(struct filter *_filter)
{
	ChainFilter *chain = (ChainFilter *)_filter;

	delete chain;
}

/**
 * Close all filters in the chain until #until is reached.  #until
 * itself is not closed.
 */
static void
chain_close_until(ChainFilter *chain, const struct filter *until)
{
	for (auto filter : chain->children) {
		if (filter == until)
			/* don't close this filter */
			return;

		/* close this filter */
		filter_close(filter);
	}

	/* this assertion fails if #until does not exist (anymore) */
	assert(false);
}

static const struct audio_format *
chain_open_child(struct filter *filter,
		 const struct audio_format *prev_audio_format,
		 GError **error_r)
{
	struct audio_format conv_audio_format = *prev_audio_format;
	const struct audio_format *next_audio_format;

	next_audio_format = filter_open(filter, &conv_audio_format, error_r);
	if (next_audio_format == NULL)
		return NULL;

	if (!audio_format_equals(&conv_audio_format, prev_audio_format)) {
		struct audio_format_string s;

		filter_close(filter);
		g_set_error(error_r, filter_quark(), 0,
			    "Audio format not supported by filter '%s': %s",
			    filter->plugin->name,
			    audio_format_to_string(prev_audio_format, &s));
		return NULL;
	}

	return next_audio_format;
}

static const struct audio_format *
chain_filter_open(struct filter *_filter, struct audio_format *in_audio_format,
		  GError **error_r)
{
	ChainFilter *chain = (ChainFilter *)_filter;
	const struct audio_format *audio_format = in_audio_format;

	for (auto filter : chain->children) {
		audio_format = chain_open_child(filter, audio_format, error_r);
		if (audio_format == NULL) {
			/* rollback, close all children */
			chain_close_until(chain, filter);
			return NULL;
		}
	}

	/* return the output format of the last filter */
	return audio_format;
}

static void
chain_filter_close(struct filter *_filter)
{
	ChainFilter *chain = (ChainFilter *)_filter;

	for (auto filter : chain->children)
		filter_close(filter);
}

static const void *
chain_filter_filter(struct filter *_filter,
		    const void *src, size_t src_size,
		    size_t *dest_size_r, GError **error_r)
{
	ChainFilter *chain = (ChainFilter *)_filter;

	for (auto filter : chain->children) {
		/* feed the output of the previous filter as input
		   into the current one */
		src = filter_filter(filter, src, src_size, &src_size, error_r);
		if (src == NULL)
			return NULL;
	}

	/* return the output of the last filter */
	*dest_size_r = src_size;
	return src;
}

const struct filter_plugin chain_filter_plugin = {
	"chain",
	chain_filter_init,
	chain_filter_finish,
	chain_filter_open,
	chain_filter_close,
	chain_filter_filter,
};

struct filter *
filter_chain_new(void)
{
	struct filter *filter = filter_new(&chain_filter_plugin, NULL, NULL);
	/* chain_filter_init() never fails */
	assert(filter != NULL);

	return filter;
}

void
filter_chain_append(struct filter *_chain, struct filter *filter)
{
	ChainFilter *chain = (ChainFilter *)_chain;

	chain->children.push_back(filter);
}
