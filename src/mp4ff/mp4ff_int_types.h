#ifndef _MP4FF_INT_TYPES_H_
#define _MP4FF_INT_TYPES_H_

#ifdef _WIN32

typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef long int32_t;
typedef unsigned long uint32_t;

typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

#else

#include "config.h"

#if defined(HAVE_STDINT_H)
#include <stdint.h>
#elif defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#elif defined(HAVE_SYS_INTTYPES_H)
#include <sys/inttypes.h>
#elif defined(HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif

#endif

#endif
