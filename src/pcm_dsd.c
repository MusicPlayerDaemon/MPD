/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "pcm_dsd.h"
#include "dsd2pcm/dsd2pcm.h"

#include <glib.h>
#include <string.h>

void
pcm_dsd_init(struct pcm_dsd *dsd)
{
	pcm_buffer_init(&dsd->buffer);

	memset(dsd->dsd2pcm, 0, sizeof(dsd->dsd2pcm));
}

void
pcm_dsd_deinit(struct pcm_dsd *dsd)
{
	pcm_buffer_deinit(&dsd->buffer);

	for (unsigned i = 0; i < G_N_ELEMENTS(dsd->dsd2pcm); ++i)
		if (dsd->dsd2pcm[i] != NULL)
			dsd2pcm_destroy(dsd->dsd2pcm[i]);
}

void
pcm_dsd_reset(struct pcm_dsd *dsd)
{
	for (unsigned i = 0; i < G_N_ELEMENTS(dsd->dsd2pcm); ++i)
		if (dsd->dsd2pcm[i] != NULL)
			dsd2pcm_reset(dsd->dsd2pcm[i]);
}

const float *
pcm_dsd_to_float(struct pcm_dsd *dsd, unsigned channels, bool lsbfirst,
		 const uint8_t *src, size_t src_size,
		 size_t *dest_size_r)
{
	assert(dsd != NULL);
	assert(src != NULL);
	assert(src_size > 0);
	assert(src_size % channels == 0);
	assert(channels <= G_N_ELEMENTS(dsd->dsd2pcm));

	const unsigned num_samples = src_size;
	const unsigned num_frames = src_size / channels;

	float *dest;
	const size_t dest_size = num_samples * sizeof(*dest);
	*dest_size_r = dest_size;
	dest = pcm_buffer_get(&dsd->buffer, dest_size);

	for (unsigned c = 0; c < channels; ++c) {
		if (dsd->dsd2pcm[c] == NULL) {
			dsd->dsd2pcm[c] = dsd2pcm_init();
			if (dsd->dsd2pcm[c] == NULL)
				return NULL;
		}

		dsd2pcm_translate(dsd->dsd2pcm[c], num_frames,
				  src + c, channels,
				  lsbfirst, dest + c, channels);
	}

	return dest;
}
