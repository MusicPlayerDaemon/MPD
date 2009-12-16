#ifndef MPD_CUE_TAG_H
#define MPD_CUE_TAG_H

#include "check.h"

#ifdef HAVE_CUE /* libcue */

#include <stdio.h>

struct tag;

struct tag*
cue_tag_file(	FILE*,
		const unsigned int);

struct tag*
cue_tag_string(	char*,
		const unsigned int);

#endif /* libcue */
#endif
