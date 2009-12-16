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
cue_tag_file(FILE *fp, unsigned tnum)
{
	struct Cd *cd;
	struct tag *cd_tag, *track_tag;

	assert(fp != NULL);

	if (tnum > 256)
		return NULL;

	cd = cue_parse_file(fp);
	if (cd == NULL)
		return NULL;

	/* tag from CDtext info */
	cd_tag = cue_tag_cd(cd_get_cdtext(cd), cd_get_rem(cd));

	/* tag from TRACKtext info */
	track_tag = cue_tag_track(track_get_cdtext(cd_get_track(cd, tnum)),
				  track_get_rem(cd_get_track(cd, tnum)));

	cd_delete(cd);

	if (cd_tag != NULL && track_tag != NULL) {
		struct tag *merge_tag = tag_merge(cd_tag, track_tag);
		tag_free(cd_tag);
		tag_free(track_tag);
		return merge_tag;
	} else if (cd_tag != NULL)
		return cd_tag;
	else if (track_tag != NULL)
		return track_tag;
	else
		return NULL;
}

struct tag *
cue_tag_string(const char *str, unsigned tnum)
{
	struct Cd *cd;
	struct tag *cd_tag, *track_tag;

	assert(str != NULL);

	if (tnum > 256)
		return NULL;

	cd = cue_parse_string(str);
	if (cd == NULL)
		return NULL;

	/* tag from CDtext info */
	cd_tag = cue_tag_cd(cd_get_cdtext(cd), cd_get_rem(cd));

	/* tag from TRACKtext info */
	track_tag = cue_tag_track(track_get_cdtext(cd_get_track(cd, tnum)),
				  track_get_rem(cd_get_track(cd, tnum)));

	cd_delete(cd);

	if (cd_tag != NULL && track_tag != NULL) {
		struct tag *merge_tag = tag_merge(cd_tag, track_tag);
		tag_free(cd_tag);
		tag_free(track_tag);
		return merge_tag;
	} else if (cd_tag != NULL)
		return cd_tag;
	else if (track_tag != NULL)
		return track_tag;
	else
		return NULL;
}
