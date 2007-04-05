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

#ifndef MPD_TYPES_H
#define MPD_TYPES_H

#include "../config.h"

typedef unsigned char mpd_uint8;
typedef signed char mpd_sint8;

#if SIZEOF_SHORT == 2
typedef unsigned short mpd_uint16;
typedef signed short mpd_sint16;
#elif SIZEOF_INT == 2
typedef unsigned int mpd_uint16;
typedef signed int mpd_sint16;
#endif

#if SIZEOF_INT == 4
typedef unsigned int mpd_uint32;
typedef signed int mpd_sint32;
#elif SIZEOF_LONG == 4
typedef unsigned long mpd_uint32;
typedef signed long mpd_sint32;
#endif

#endif
