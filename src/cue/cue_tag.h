#ifndef MPD_CUE_TAG_H
#define MPD_CUE_TAG_H

#include "check.h"

#ifdef HAVE_CUE /* libcue */

#include "tag.h"

#include <libcue/libcue.h>

struct tag*
cue_tag_file(	FILE*,
		const unsigned int);

struct tag*
cue_tag_string(	char*,
		const unsigned int);

#endif /* libcue */
#endif
