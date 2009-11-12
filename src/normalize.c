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
#include "normalize.h"
#include "compress.h"
#include "conf.h"
#include "audio_format.h"

#define DEFAULT_VOLUME_NORMALIZATION 0

int normalizationEnabled;

void initNormalization(void)
{
	normalizationEnabled = config_get_bool(CONF_VOLUME_NORMALIZATION,
					       DEFAULT_VOLUME_NORMALIZATION);

	if (normalizationEnabled)
		CompressCfg(0, ANTICLIP, TARGET, GAINMAX, GAINSMOOTH, BUCKETS);
}

void finishNormalization(void)
{
	if (normalizationEnabled) CompressFree();
}

void normalizeData(char *buffer, int bufferSize,
		   const struct audio_format *format)
{
	if ((format->bits != 16) || (format->channels != 2)) return;

	CompressDo(buffer, bufferSize);
}
