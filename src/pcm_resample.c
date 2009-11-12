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

#include "config.h"
#include "pcm_resample_internal.h"

#ifdef HAVE_LIBSAMPLERATE
#include "conf.h"
#endif

#include <string.h>

#ifdef HAVE_LIBSAMPLERATE
static bool
pcm_resample_lsr_enabled(void)
{
	return strcmp(config_get_string(CONF_SAMPLERATE_CONVERTER, ""),
		      "internal") != 0;
}
#endif

void pcm_resample_init(struct pcm_resample_state *state)
{
	memset(state, 0, sizeof(*state));

#ifdef HAVE_LIBSAMPLERATE
	if (pcm_resample_lsr_enabled()) {
		pcm_buffer_init(&state->in);
		pcm_buffer_init(&state->out);
	}
#endif

	pcm_buffer_init(&state->buffer);
}

void pcm_resample_deinit(struct pcm_resample_state *state)
{
#ifdef HAVE_LIBSAMPLERATE
	if (pcm_resample_lsr_enabled())
		pcm_resample_lsr_deinit(state);
	else
#endif
		pcm_resample_fallback_deinit(state);
}

const int16_t *
pcm_resample_16(struct pcm_resample_state *state,
		uint8_t channels,
		unsigned src_rate, const int16_t *src_buffer, size_t src_size,
		unsigned dest_rate, size_t *dest_size_r,
		GError **error_r)
{
#ifdef HAVE_LIBSAMPLERATE
	if (pcm_resample_lsr_enabled())
		return pcm_resample_lsr_16(state, channels,
					   src_rate, src_buffer, src_size,
					   dest_rate, dest_size_r,
					   error_r);
#else
	(void)error_r;
#endif

	return pcm_resample_fallback_16(state, channels,
					src_rate, src_buffer, src_size,
					dest_rate, dest_size_r);
}

const int32_t *
pcm_resample_32(struct pcm_resample_state *state,
		uint8_t channels,
		unsigned src_rate, const int32_t *src_buffer, size_t src_size,
		unsigned dest_rate, size_t *dest_size_r,
		GError **error_r)
{
#ifdef HAVE_LIBSAMPLERATE
	if (pcm_resample_lsr_enabled())
		return pcm_resample_lsr_32(state, channels,
					   src_rate, src_buffer, src_size,
					   dest_rate, dest_size_r,
					   error_r);
#else
	(void)error_r;
#endif

	return pcm_resample_fallback_32(state, channels,
					src_rate, src_buffer, src_size,
					dest_rate, dest_size_r);
}
