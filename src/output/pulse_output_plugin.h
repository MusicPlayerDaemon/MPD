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

#ifndef MPD_PULSE_OUTPUT_PLUGIN_H
#define MPD_PULSE_OUTPUT_PLUGIN_H

#include <stdbool.h>
#include <stddef.h>

#include <glib.h>

#include <pulse/version.h>

#if !defined(PA_CHECK_VERSION)
/**
 * This macro was implemented in libpulse 0.9.16.
 */
#define PA_CHECK_VERSION(a,b,c) false
#endif

struct pa_operation;
struct pa_cvolume;

struct pulse_output {
	const char *name;
	const char *server;
	const char *sink;

	struct pulse_mixer *mixer;

	struct pa_threaded_mainloop *mainloop;
	struct pa_context *context;
	struct pa_stream *stream;

	size_t writable;

#if !PA_CHECK_VERSION(0,9,11)
	/**
	 * We need this variable because pa_stream_is_corked() wasn't
	 * added before 0.9.11.
	 */
	bool pause;
#endif
};

void
pulse_output_set_mixer(struct pulse_output *po, struct pulse_mixer *pm);

void
pulse_output_clear_mixer(struct pulse_output *po, struct pulse_mixer *pm);

bool
pulse_output_set_volume(struct pulse_output *po,
			const struct pa_cvolume *volume, GError **error_r);

#endif
