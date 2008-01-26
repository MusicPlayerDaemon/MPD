/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
 *
 * Common functions used for Ogg data streams (Ogg-Vorbis and OggFLAC)
 * (c) 2005 by Eric Wong <normalperson@yhbt.net>
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

#include "../inputPlugin.h"

#include "_flac_common.h"
#include "_ogg_common.h"

#if defined(HAVE_OGGFLAC) || defined(HAVE_OGGVORBIS) || \
  (defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7)

#include "../utils.h"

#include <string.h>

ogg_stream_type ogg_stream_type_detect(InputStream * inStream)
{
	/* oggflac detection based on code in ogg123 and this post
	 * http://lists.xiph.org/pipermail/flac/2004-December/000393.html
	 * ogg123 trunk still doesn't have this patch as of June 2005 */
	unsigned char buf[41];
	size_t r, to_read = 41;

	seekInputStream(inStream, 0, SEEK_SET);

	while (to_read) {
		r = readFromInputStream(inStream, buf, 1, to_read);
		if (inStream->error)
			break;
		to_read -= r;
		if (!r && !inputStreamAtEOF(inStream))
			my_usleep(10000);
		else
			break;
	}

	seekInputStream(inStream, 0, SEEK_SET);

	if (r >= 32 && memcmp(buf, "OggS", 4) == 0 && ((memcmp
							(buf + 29, "FLAC",
							 4) == 0
							&& memcmp(buf + 37,
								  "fLaC",
								  4) == 0)
						       ||
						       (memcmp
							(buf + 28, "FLAC",
							 4) == 0)
						       ||
						       (memcmp
							(buf + 28, "fLaC",
							 4) == 0))) {
		return FLAC;
	}
	return VORBIS;
}

#endif /* defined(HAVE_OGGFLAC || defined(HAVE_OGGVORBIS) */
