#include "config.h"
#include "cue_tag.h"
#include "tag.h"

#include <libcue/libcue.h>
#include <assert.h>

static struct tag *
cue_tag_cd(struct Cdtext *cdtext, struct Rem *rem)
{
	struct tag *tag;
	char *tmp;

	assert(cdtext != NULL);

	tag = tag_new();

	tag_begin_add(tag);

	/* TAG_ALBUM_ARTIST */
	if ((tmp = cdtext_get(PTI_PERFORMER, cdtext)) != NULL)
		tag_add_item(tag, TAG_ALBUM_ARTIST, tmp);

	else if ((tmp = cdtext_get(PTI_SONGWRITER, cdtext)) != NULL)
		tag_add_item(tag, TAG_ALBUM_ARTIST, tmp);

	else if ((tmp = cdtext_get(PTI_COMPOSER, cdtext)) != NULL)
		tag_add_item(tag, TAG_ALBUM_ARTIST, tmp);

	else if ((tmp = cdtext_get(PTI_ARRANGER, cdtext)) != NULL)
		tag_add_item(tag, TAG_ALBUM_ARTIST, tmp);

	/* TAG_ARTIST */
	if ((tmp = cdtext_get(PTI_PERFORMER, cdtext)) != NULL)
		tag_add_item(tag, TAG_ARTIST, tmp);

	else if ((tmp = cdtext_get(PTI_SONGWRITER, cdtext)) != NULL)
		tag_add_item(tag, TAG_ARTIST, tmp);

	else if ((tmp = cdtext_get(PTI_COMPOSER, cdtext)) != NULL)
		tag_add_item(tag, TAG_ARTIST, tmp);

	else if ((tmp = cdtext_get(PTI_ARRANGER, cdtext)) != NULL)
		tag_add_item(tag, TAG_ARTIST, tmp);

	/* TAG_PERFORMER */
	if ((tmp = cdtext_get(PTI_PERFORMER, cdtext)) != NULL)
		tag_add_item(tag, TAG_PERFORMER, tmp);

	/* TAG_COMPOSER */
	if ((tmp = cdtext_get(PTI_COMPOSER, cdtext)) != NULL)
		tag_add_item(tag, TAG_COMPOSER, tmp);

	/* TAG_ALBUM */
	if ((tmp = cdtext_get(PTI_TITLE, cdtext)) != NULL)
		tag_add_item(tag, TAG_ALBUM, tmp);

	/* TAG_GENRE */
	if ((tmp = cdtext_get(PTI_GENRE, cdtext)) != NULL)
		tag_add_item(tag, TAG_GENRE, tmp);

	/* TAG_DATE */
	if ((tmp = rem_get(REM_DATE, rem)) != NULL)
		tag_add_item(tag, TAG_DATE, tmp);

	/* TAG_COMMENT */
	if ((tmp = cdtext_get(PTI_MESSAGE, cdtext)) != NULL)
		tag_add_item(tag, TAG_COMMENT, tmp);

	/* TAG_DISC */
	if ((tmp = cdtext_get(PTI_DISC_ID, cdtext)) != NULL)
		tag_add_item(tag, TAG_DISC, tmp);

	/* stream name, usually empty
	 * tag_add_item(tag, TAG_NAME,);
	 */

	/* REM MUSICBRAINZ entry?
	tag_add_item(tag, TAG_MUSICBRAINZ_ARTISTID,);
	tag_add_item(tag, TAG_MUSICBRAINZ_ALBUMID,);
	tag_add_item(tag, TAG_MUSICBRAINZ_ALBUMARTISTID,);
	tag_add_item(tag, TAG_MUSICBRAINZ_TRACKID,);
	*/

	tag_end_add(tag);

	if (tag_is_empty(tag)) {
		tag_free(tag);
		return NULL;
	}

	return tag;
}

static struct tag *
cue_tag_track(struct Cdtext *cdtext, struct Rem *rem)
{
	struct tag *tag;
	char *tmp;

	assert(cdtext != NULL);

	tag = tag_new();

	tag_begin_add(tag);

	/* TAG_ARTIST */
	if ((tmp = cdtext_get(PTI_PERFORMER, cdtext)) != NULL)
		tag_add_item(tag, TAG_ARTIST, tmp);

	else if ((tmp = cdtext_get(PTI_SONGWRITER, cdtext)) != NULL)
		tag_add_item(tag, TAG_ARTIST, tmp);

	else if ((tmp = cdtext_get(PTI_COMPOSER, cdtext)) != NULL)
		tag_add_item(tag, TAG_ARTIST, tmp);

	else if ((tmp = cdtext_get(PTI_ARRANGER, cdtext)) != NULL)
		tag_add_item(tag, TAG_ARTIST, tmp);

	/* TAG_TITLE */
	if ((tmp = cdtext_get(PTI_TITLE, cdtext)) != NULL)
		tag_add_item(tag, TAG_TITLE, tmp);

	/* TAG_GENRE */
	if ((tmp = cdtext_get(PTI_GENRE, cdtext)) != NULL)
		tag_add_item(tag, TAG_GENRE, tmp);

	/* TAG_DATE */
	if ((tmp = rem_get(REM_DATE, rem)) != NULL)
		tag_add_item(tag, TAG_DATE, tmp);

	/* TAG_COMPOSER */
	if ((tmp = cdtext_get(PTI_COMPOSER, cdtext)) != NULL)
		tag_add_item(tag, TAG_COMPOSER, tmp);

	/* TAG_PERFORMER */
	if ((tmp = cdtext_get(PTI_PERFORMER, cdtext)) != NULL)
		tag_add_item(tag, TAG_PERFORMER, tmp);

	/* TAG_COMMENT */
	if ((tmp = cdtext_get(PTI_MESSAGE, cdtext)) != NULL)
		tag_add_item(tag, TAG_COMMENT, tmp);

	/* TAG_DISC */
	if ((tmp = cdtext_get(PTI_DISC_ID, cdtext)) != NULL)
		tag_add_item(tag, TAG_DISC, tmp);

	tag_end_add(tag);

	if (tag_is_empty(tag)) {
		tag_free(tag);
		return NULL;
	}

	return tag;
}

struct tag *
cue_tag(struct Cd *cd, unsigned tnum)
{
	struct tag *cd_tag, *track_tag, *tag;
	struct Track *track;

	assert(cd != NULL);

	track = cd_get_track(cd, tnum);
	if (track == NULL)
		return NULL;

	/* tag from CDtext info */
	cd_tag = cue_tag_cd(cd_get_cdtext(cd), cd_get_rem(cd));

	/* tag from TRACKtext info */
	track_tag = cue_tag_track(track_get_cdtext(track),
				  track_get_rem(track));

	tag = tag_merge_replace(cd_tag, track_tag);
	if (tag == NULL)
		return NULL;

	/* Create a tag number */

	tag_clear_items_by_type(tag, TAG_TRACK);

	char convert_uinttostring[8];
	snprintf(convert_uinttostring, sizeof(convert_uinttostring),
		 "%02d/%02d", tnum, cd_get_ntrack(cd));
	tag_add_item(tag, TAG_TRACK, convert_uinttostring);

	tag->time = track_get_length(track)
	    - track_get_index(track, 1)
	    + track_get_zero_pre(track);
	track = cd_get_track(cd, tnum + 1);
	if (track != NULL)
		tag->time += track_get_index(track, 1)
		    - track_get_zero_pre(track);
	/* libcue returns the track duration in frames, and there are
	   75 frames per second; this formula rounds down */
	tag->time = tag->time / 75;

	return tag;
}

struct tag *
cue_tag_file(FILE *fp, unsigned tnum)
{
	struct Cd *cd;
	struct tag *tag;

	assert(fp != NULL);

	if (tnum > 256)
		return NULL;

	cd = cue_parse_file(fp);
	if (cd == NULL)
		return NULL;

	tag = cue_tag(cd, tnum);
	cd_delete(cd);

	return tag;
}

struct tag *
cue_tag_string(const char *str, unsigned tnum)
{
	struct Cd *cd;
	struct tag *tag;

	assert(str != NULL);

	if (tnum > 256)
		return NULL;

	cd = cue_parse_string(str);
	if (cd == NULL)
		return NULL;

	tag = cue_tag(cd, tnum);
	cd_delete(cd);

	return tag;
}
