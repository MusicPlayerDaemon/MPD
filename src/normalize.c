/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "compress.h"
#include "conf.h"
#include "normalize.h"

#include <stdlib.h>

int normalizationEnabled;

void initNormalization(void)
{
	normalizationEnabled = getBoolConfigParam(CONF_VOLUME_NORMALIZATION);
	if (normalizationEnabled == -1) normalizationEnabled = 0;
	else if (normalizationEnabled < 0) exit(EXIT_FAILURE);

	if (normalizationEnabled)
		CompressCfg(0, ANTICLIP, TARGET, GAINMAX, GAINSMOOTH, BUCKETS);
}

void finishNormalization(void)
{
	if (normalizationEnabled) CompressFree();
}

void normalizeData(char *buffer, int bufferSize, AudioFormat *format)
{
	if ((format->bits != 16) || (format->channels != 2)) return;

	CompressDo(buffer, bufferSize);
}
