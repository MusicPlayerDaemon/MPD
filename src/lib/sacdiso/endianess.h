/**
 * SACD Ripper - http://code.google.com/p/sacd-ripper/
 *
 * Copyright (c) 2010-2011 by respective authors.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _ENDIANESS_H_INCLUDED
#define _ENDIANESS_H_INCLUDED

#define MAKE_MARKER(a, b, c, d) ((a)|((b)<<8)|((c)<<16)|((d)<<24))

#define hton16(x)            \
    ((((x) & 0xff00) >> 8) | \
     (((x) & 0x00ff) << 8))
#define hton32(x)                 \
    ((((x) & 0xff000000) >> 24) | \
     (((x) & 0x00ff0000) >> 8) |  \
     (((x) & 0x0000ff00) << 8) |  \
     (((x) & 0x000000ff) << 24))
#define hton64(x)                            \
    ((((x) & 0xff00000000000000ULL) >> 56) | \
     (((x) & 0x00ff000000000000ULL) >> 40) | \
     (((x) & 0x0000ff0000000000ULL) >> 24) | \
     (((x) & 0x000000ff00000000ULL) >> 8) |  \
     (((x) & 0x00000000ff000000ULL) << 8) |  \
     (((x) & 0x0000000000ff0000ULL) << 24) | \
     (((x) & 0x000000000000ff00ULL) << 40) | \
     (((x) & 0x00000000000000ffULL) << 56))

#define SWAP16(x) x = (hton16(x))
#define SWAP32(x) x = (hton32(x))
#define SWAP64(x) x = (hton64(x))

#endif
