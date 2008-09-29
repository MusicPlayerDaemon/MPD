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

#if defined(HAVE_INTTYPES_H)
  /*
   * inttypes.h pulls in stdint.h on C99 systems, needed for older systems
   * that didn't provide stdint.h but still defined equivalent types.
   */
#  include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#  include <stdint.h>
#elif defined(HAVE_SYS_INTTYPES_H)
#  include <sys/inttypes.h> /* some ancient systems had this, untested */
#endif /* C99-ish type headers */

#include <sys/types.h>

#if (!defined(HAVE_STDINT_H) && !defined(HAVE_INTTYPES_H))

/*
 * this only includes a partial subset of what is expected in a C99
 * stdint.h or inttypes.h; but includes enough of what is needed for mpd
 * to function on older platforms
 * (especially Linux ones still using gcc 2.95)
 */

typedef unsigned char uint8_t;
typedef signed char int8_t;

#if SIZEOF_SHORT == 2
typedef unsigned short uint16_t;
typedef signed short int16_t;
#elif SIZEOF_INT == 2
typedef unsigned int uint16_t;
typedef signed int int16_t;
#endif /* (u)int_16_t */

#if SIZEOF_INT == 4
typedef unsigned int uint32_t;
typedef signed int int32_t;
#elif SIZEOF_LONG == 4
typedef unsigned long uint32_t;
typedef signed long int32_t;
#endif /* (u)int_32 */

#endif /* !HAVE_STDINT_H && !HAVE_INTTYPES_H */

#endif
