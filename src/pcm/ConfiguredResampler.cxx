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
#include "ConfiguredResampler.hxx"
#include "FallbackResampler.hxx"

#ifdef HAVE_LIBSAMPLERATE
#include "LibsamplerateResampler.hxx"
#include "ConfigGlobal.hxx"
#include "ConfigOption.hxx"
#endif

#include <string.h>

#ifdef HAVE_LIBSAMPLERATE
static bool lsr_enabled;
#endif

bool
pcm_resampler_global_init(Error &error)
{
#ifdef HAVE_LIBSAMPLERATE
	const char *converter =
		config_get_string(CONF_SAMPLERATE_CONVERTER, "");

	lsr_enabled = strcmp(converter, "internal") != 0;
	if (lsr_enabled)
		return pcm_resample_lsr_global_init(converter, error);
	else
		return true;
#else
	(void)error;
	return true;
#endif
}

PcmResampler *
pcm_resampler_create()
{
#ifdef HAVE_LIBSAMPLERATE
	if (lsr_enabled)
		return new LibsampleratePcmResampler();
#endif

	return new FallbackPcmResampler();
}
