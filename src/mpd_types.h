#ifndef MPD_TYPES_H
#define MPD_TYPES_H

typedef unsigned char	mpd_uint8;
typedef signed char	mpd_sint8;

#if SIZEOF_SHORT == 2
typedef unsigned short 	mpd_uint16;
typedef signed short	mpd_sint16;
#elif SIZEOF_INT == 2
typedef unsigned short 	mpd_uint16;
typedef signed short	mpd_sint16;
#endif

#if SIZEOF_INT == 4
typedef unsigned int 	mpd_uint32;
typedef signed int	mpd_sint32;
#elif SIZEOF_LONG == 4
typedef unsigned int 	mpd_uint32;
typedef signed int	mpd_sint32;
#endif

#endif
