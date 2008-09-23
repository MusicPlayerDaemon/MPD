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

#include "song_save.h"
#include "tag_save.h"
#include "directory.h"
#include "path.h"
#include "utils.h"
#include "tag.h"
#include "log.h"

#define SONG_KEY	"key: "
#define SONG_MTIME	"mtime: "

static void song_save_url(FILE *fp, Song * song)
{
	if (song->parentDir != NULL && song->parentDir->path != NULL)
		fprintf(fp, SONG_FILE "%s/%s\n",
			getDirectoryPath(song->parentDir), song->url);
	else
		fprintf(fp, SONG_FILE "%s\n",
			song->url);
}

static void song_save(FILE *fp, Song * song)
{
	song_save_url(fp, song);

	if (song->tag != NULL)
		tag_save(fp, song->tag);
}

void songvec_save(FILE *fp, struct songvec *sv)
{
	int i;
	Song **sp = sv->base;

	fprintf(fp, "%s\n", SONG_BEGIN);

	for (i = sv->nr; --i >= 0; ) {
		Song *song = *sp++;
		fprintf(fp, "%s%s\n", SONG_KEY, song->url);
		song_save(fp, song);
		fprintf(fp, "%s%li\n", SONG_MTIME, (long)song->mtime);
	}

	fprintf(fp, "%s\n", SONG_END);
}

static void insertSongIntoList(struct songvec *sv, Song *newsong)
{
	Song *existing = songvec_find(sv, newsong->url);

	if (!existing) {
		songvec_add(sv, newsong);
		if (newsong->tag)
			tag_end_add(newsong->tag);
	} else { /* prevent dupes, just update the existing song info */
		if (existing->mtime != newsong->mtime) {
			tag_free(existing->tag);
			if (newsong->tag)
				tag_end_add(newsong->tag);
			existing->tag = newsong->tag;
			existing->mtime = newsong->mtime;
			newsong->tag = NULL;
		}
		freeJustSong(newsong);
	}
}

static int matchesAnMpdTagItemKey(char *buffer, int *itemType)
{
	int i;

	for (i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		if (0 == strncmp(mpdTagItemKeys[i], buffer,
				 strlen(mpdTagItemKeys[i]))) {
			*itemType = i;
			return 1;
		}
	}

	return 0;
}

void readSongInfoIntoList(FILE *fp, struct songvec *sv,
			  Directory *parentDir)
{
	char buffer[MPD_PATH_MAX + 1024];
	int bufferSize = MPD_PATH_MAX + 1024;
	Song *song = NULL;
	int itemType;

	while (myFgets(buffer, bufferSize, fp) && 0 != strcmp(SONG_END, buffer)) {
		if (0 == strncmp(SONG_KEY, buffer, strlen(SONG_KEY))) {
			if (song)
				insertSongIntoList(sv, song);

			song = newNullSong();
			song->url = xstrdup(buffer + strlen(SONG_KEY));
			song->type = SONG_TYPE_FILE;
			song->parentDir = parentDir;
		} else if (*buffer == 0) {
			/* ignore empty lines (starting with '\0') */
		} else if (song == NULL) {
			FATAL("Problems reading song info\n");
		} else if (0 == strncmp(SONG_FILE, buffer, strlen(SONG_FILE))) {
			/* we don't need this info anymore
			   song->url = xstrdup(&(buffer[strlen(SONG_FILE)]));
			 */
		} else if (matchesAnMpdTagItemKey(buffer, &itemType)) {
			if (!song->tag) {
				song->tag = tag_new();
				tag_begin_add(song->tag);
			}

			tag_add_item(song->tag, itemType,
				     &(buffer
				       [strlen(mpdTagItemKeys[itemType]) +
					2]));
		} else if (0 == strncmp(SONG_TIME, buffer, strlen(SONG_TIME))) {
			if (!song->tag) {
				song->tag = tag_new();
				tag_begin_add(song->tag);
			}

			song->tag->time = atoi(&(buffer[strlen(SONG_TIME)]));
		} else if (0 == strncmp(SONG_MTIME, buffer, strlen(SONG_MTIME))) {
			song->mtime = atoi(&(buffer[strlen(SONG_MTIME)]));
		}
		else
			FATAL("songinfo: unknown line in db: %s\n", buffer);
	}

	if (song)
		insertSongIntoList(sv, song);

	songvec_prune(sv);
}
