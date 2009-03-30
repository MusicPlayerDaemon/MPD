#ifndef MPD_CUE_TAG_H
#define MPD_CUE_TAG_H

#include "config.h"

#ifdef HAVE_CUE /* libcue */

#include <libcue/libcue.h>
#include "../tag.h"

struct tag*
cue_tag_file(	FILE*,
		const unsigned int);

struct tag*
cue_tag_string(	char*,
		const unsigned int);

#endif /* libcue */
#endif
