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

#include "directory.h"
#include "song.h"
#include "conf.h"
#include "log.h"
#include "ls.h"
#include "path.h"
#include "stats.h"
#include "utils.h"
#include "client.h"
#include "dbUtils.h"
#include "song_print.h"
#include "song_save.h"
#include "dirvec.h"
#include "update.h"

#define DIRECTORY_DIR		"directory: "
#define DIRECTORY_MTIME		"mtime: " /* DEPRECATED, noop-read-only */
#define DIRECTORY_BEGIN		"begin: "
#define DIRECTORY_END		"end: "
#define DIRECTORY_INFO_BEGIN	"info_begin"
#define DIRECTORY_INFO_END	"info_end"
#define DIRECTORY_MPD_VERSION	"mpd_version: "
#define DIRECTORY_FS_CHARSET	"fs_charset: "

static struct directory *music_root;

static time_t directory_dbModTime;

static void deleteEmptyDirectoriesInDirectory(struct directory * directory);

static char *getDbFile(void)
{
	ConfigParam *param = parseConfigFilePath(CONF_DB_FILE, 1);

	assert(param);
	assert(param->value);

	return param->value;
}

struct directory *
newDirectory(const char *dirname, struct directory * parent)
{
	struct directory *directory;

	directory = xcalloc(1, sizeof(*directory));

	if (dirname && strlen(dirname))
		directory->path = xstrdup(dirname);
	directory->parent = parent;

	return directory;
}

void
freeDirectory(struct directory * directory)
{
	dirvec_destroy(&directory->children);
	songvec_destroy(&directory->songs);
	if (directory->path)
		free(directory->path);
	free(directory);
	/* this resets last dir returned */
	/*getDirectoryPath(NULL); */
}

static void deleteEmptyDirectoriesInDirectory(struct directory * directory)
{
	int i;
	struct dirvec *dv = &directory->children;

	for (i = dv->nr; --i >= 0; ) {
		deleteEmptyDirectoriesInDirectory(dv->base[i]);
		if (directory_is_empty(dv->base[i]))
			dirvec_delete(dv, dv->base[i]);
	}
	if (!dv->nr)
		dirvec_destroy(dv);
}

void directory_finish(void)
{
	freeDirectory(music_root);
}

int isRootDirectory(const char *name)
{
	return (!name || name[0] == '\0' || !strcmp(name, "/"));
}

struct directory *
directory_get_root(void)
{
	assert(music_root != NULL);

	return music_root;
}

static struct directory *
getSubDirectory(struct directory * directory, const char *name)
{
	struct directory *cur = directory;
	struct directory *found = NULL;
	char *duplicated;
	char *locate;

	if (isRootDirectory(name))
		return directory;

	duplicated = xstrdup(name);
	locate = strchr(duplicated, '/');
	while (1) {
		if (locate)
			*locate = '\0';
		if (!(found = dirvec_find(&cur->children, duplicated)))
			break;
		assert(cur == found->parent);
		cur = found;
		if (!locate)
			break;
		*locate = '/';
		locate = strchr(locate + 1, '/');
	}

	free(duplicated);

	return found;
}

struct directory *
getDirectory(const char *name)
{
	return getSubDirectory(music_root, name);
}

static int printDirectoryList(struct client *client, struct dirvec *dv)
{
	size_t i;

	for (i = 0; i < dv->nr; ++i) {
		client_printf(client, DIRECTORY_DIR "%s\n",
			      getDirectoryPath(dv->base[i]));
	}

	return 0;
}

int printDirectoryInfo(struct client *client, const char *name)
{
	struct directory *directory;

	if ((directory = getDirectory(name)) == NULL)
		return -1;

	printDirectoryList(client, &directory->children);
	songvec_print(client, &directory->songs);

	return 0;
}

/* TODO error checking */
static int
writeDirectoryInfo(FILE * fp, struct directory * directory)
{
	struct dirvec *children = &directory->children;
	size_t i;
	int retv;

	if (directory->path) {
		retv = fprintf(fp, "%s%s\n", DIRECTORY_BEGIN,
			       getDirectoryPath(directory));
		if (retv < 0)
			return -1;
	}

	for (i = 0; i < children->nr; ++i) {
		struct directory *cur = children->base[i];
		const char *base = mpd_basename(cur->path);

		retv = fprintf(fp, DIRECTORY_DIR "%s\n", base);
		if (retv < 0)
			return -1;
		if (writeDirectoryInfo(fp, cur) < 0)
			return -1;
	}

	songvec_save(fp, &directory->songs);

	if (directory->path &&
	    fprintf(fp, DIRECTORY_END "%s\n",
		    getDirectoryPath(directory)) < 0)
		return -1;
	return 0;
}

static void
readDirectoryInfo(FILE * fp, struct directory * directory)
{
	char buffer[MPD_PATH_MAX * 2];
	int bufferSize = MPD_PATH_MAX * 2;
	char key[MPD_PATH_MAX * 2];
	char *name;

	while (myFgets(buffer, bufferSize, fp)
	       && prefixcmp(buffer, DIRECTORY_END)) {
		if (!prefixcmp(buffer, DIRECTORY_DIR)) {
			struct directory *subdir;

			strcpy(key, &(buffer[strlen(DIRECTORY_DIR)]));
			if (!myFgets(buffer, bufferSize, fp))
				FATAL("Error reading db, fgets\n");
			/* for compatibility with db's prior to 0.11 */
			if (!prefixcmp(buffer, DIRECTORY_MTIME)) {
				if (!myFgets(buffer, bufferSize, fp))
					FATAL("Error reading db, fgets\n");
			}
			if (prefixcmp(buffer, DIRECTORY_BEGIN))
				FATAL("Error reading db at line: %s\n", buffer);
			name = &(buffer[strlen(DIRECTORY_BEGIN)]);
			if ((subdir = getDirectory(name))) {
				assert(subdir->parent == directory);
			} else {
				subdir = newDirectory(name, directory);
				dirvec_add(&directory->children, subdir);
			}
			readDirectoryInfo(fp, subdir);
		} else if (!prefixcmp(buffer, SONG_BEGIN)) {
			readSongInfoIntoList(fp, &directory->songs, directory);
		} else {
			FATAL("Unknown line in db: %s\n", buffer);
		}
	}
}

void
sortDirectory(struct directory * directory)
{
	int i;
	struct dirvec *dv = &directory->children;

	dirvec_sort(dv);
	songvec_sort(&directory->songs);

	for (i = dv->nr; --i >= 0; )
		sortDirectory(dv->base[i]);
}

int checkDirectoryDB(void)
{
	struct stat st;
	char *dbFile = getDbFile();

	/* Check if the file exists */
	if (access(dbFile, F_OK)) {
		/* If the file doesn't exist, we can't check if we can write
		 * it, so we are going to try to get the directory path, and
		 * see if we can write a file in that */
		char dirPath[MPD_PATH_MAX];
		parent_path(dirPath, dbFile);
		if (*dirPath == '\0')
			strcpy(dirPath, "/");

		/* Check that the parent part of the path is a directory */
		if (stat(dirPath, &st) < 0) {
			ERROR("Couldn't stat parent directory of db file "
			      "\"%s\": %s\n", dbFile, strerror(errno));
			return -1;
		}

		if (!S_ISDIR(st.st_mode)) {
			ERROR("Couldn't create db file \"%s\" because the "
			      "parent path is not a directory\n", dbFile);
			return -1;
		}

		/* Check if we can write to the directory */
		if (access(dirPath, R_OK | W_OK)) {
			ERROR("Can't create db file in \"%s\": %s\n", dirPath,
			      strerror(errno));
			return -1;
		}

		return 0;
	}

	/* Path exists, now check if it's a regular file */
	if (stat(dbFile, &st) < 0) {
		ERROR("Couldn't stat db file \"%s\": %s\n", dbFile,
		      strerror(errno));
		return -1;
	}

	if (!S_ISREG(st.st_mode)) {
		ERROR("db file \"%s\" is not a regular file\n", dbFile);
		return -1;
	}

	/* And check that we can write to it */
	if (access(dbFile, R_OK | W_OK)) {
		ERROR("Can't open db file \"%s\" for reading/writing: %s\n",
		      dbFile, strerror(errno));
		return -1;
	}

	return 0;
}

int writeDirectoryDB(void)
{
	FILE *fp;
	char *dbFile = getDbFile();
	struct stat st;

	DEBUG("removing empty directories from DB\n");
	deleteEmptyDirectoriesInDirectory(music_root);

	DEBUG("sorting DB\n");

	sortDirectory(music_root);

	DEBUG("writing DB\n");

	fp = fopen(dbFile, "w");
	if (!fp) {
		ERROR("unable to write to db file \"%s\": %s\n",
		      dbFile, strerror(errno));
		return -1;
	}

	/* block signals when writing the db so we don't get a corrupted db */
	fprintf(fp, "%s\n", DIRECTORY_INFO_BEGIN);
	fprintf(fp, "%s%s\n", DIRECTORY_MPD_VERSION, VERSION);
	fprintf(fp, "%s%s\n", DIRECTORY_FS_CHARSET, getFsCharset());
	fprintf(fp, "%s\n", DIRECTORY_INFO_END);

	if (writeDirectoryInfo(fp, music_root) < 0) {
		ERROR("Failed to write to database file: %s\n",
		      strerror(errno));
		while (fclose(fp) && errno == EINTR);
		return -1;
	}

	while (fclose(fp) && errno == EINTR);

	if (stat(dbFile, &st) == 0)
		directory_dbModTime = st.st_mtime;

	return 0;
}

int readDirectoryDB(void)
{
	FILE *fp = NULL;
	char *dbFile = getDbFile();
	struct stat st;

	if (!music_root)
		music_root = newDirectory(NULL, NULL);
	while (!(fp = fopen(dbFile, "r")) && errno == EINTR) ;
	if (fp == NULL) {
		ERROR("unable to open db file \"%s\": %s\n",
		      dbFile, strerror(errno));
		return -1;
	}

	/* get initial info */
	{
		char buffer[100];
		int bufferSize = 100;
		int foundFsCharset = 0;
		int foundVersion = 0;

		if (!myFgets(buffer, bufferSize, fp))
			FATAL("Error reading db, fgets\n");
		if (0 == strcmp(DIRECTORY_INFO_BEGIN, buffer)) {
			while (myFgets(buffer, bufferSize, fp) &&
			       0 != strcmp(DIRECTORY_INFO_END, buffer)) {
				if (!prefixcmp(buffer, DIRECTORY_MPD_VERSION))
				{
					if (foundVersion)
						FATAL("already found version in db\n");
					foundVersion = 1;
				} else if (!prefixcmp(buffer,
				                      DIRECTORY_FS_CHARSET)) {
					char *fsCharset;
					char *tempCharset;

					if (foundFsCharset)
						FATAL("already found fs charset in db\n");

					foundFsCharset = 1;

					fsCharset = &(buffer[strlen(DIRECTORY_FS_CHARSET)]);
					if ((tempCharset = getConfigParamValue(CONF_FS_CHARSET))
					    && strcmp(fsCharset, tempCharset)) {
						WARNING("Using \"%s\" for the "
							"filesystem charset "
							"instead of \"%s\"\n",
							fsCharset, tempCharset);
						WARNING("maybe you need to "
							"recreate the db?\n");
						setFsCharset(fsCharset);
					}
				} else {
					FATAL("directory: unknown line in db info: %s\n",
					     buffer);
				}
			}
		} else {
			ERROR("db info not found in db file\n");
			ERROR("you should recreate the db using --create-db\n");
			while (fclose(fp) && errno == EINTR) ;
			return -1;
		}
	}

	DEBUG("reading DB\n");

	readDirectoryInfo(fp, music_root);
	while (fclose(fp) && errno == EINTR) ;

	stats.numberOfSongs = countSongsIn(NULL);
	stats.dbPlayTime = sumSongTimesIn(NULL);

	if (stat(dbFile, &st) == 0)
		directory_dbModTime = st.st_mtime;

	return 0;
}

static int
traverseAllInSubDirectory(struct directory * directory,
			  int (*forEachSong) (struct song *, void *),
			  int (*forEachDir) (struct directory *, void *),
			  void *data)
{
	struct dirvec *dv = &directory->children;
	int err = 0;
	size_t j;

	if (forEachDir && (err = forEachDir(directory, data)) < 0)
		return err;

	if (forEachSong) {
		err = songvec_for_each(&directory->songs, forEachSong, data);
		if (err < 0)
			return err;
	}

	for (j = 0; err >= 0 && j < dv->nr; ++j)
		err = traverseAllInSubDirectory(dv->base[j], forEachSong,
						forEachDir, data);

	return err;
}

int
traverseAllIn(const char *name,
	      int (*forEachSong) (struct song *, void *),
	      int (*forEachDir) (struct directory *, void *), void *data)
{
	struct directory *directory;

	if ((directory = getDirectory(name)) == NULL) {
		struct song *song;
		if ((song = getSongFromDB(name)) && forEachSong) {
			return forEachSong(song, data);
		}
		return -1;
	}

	return traverseAllInSubDirectory(directory, forEachSong, forEachDir,
					 data);
}

void directory_init(void)
{
	music_root = newDirectory(NULL, NULL);
	updateDirectory(music_root);
	stats.numberOfSongs = countSongsIn(NULL);
	stats.dbPlayTime = sumSongTimesIn(NULL);
}

struct song *
getSongFromDB(const char *file)
{
	struct song *song = NULL;
	struct directory *directory;
	char *dir = NULL;
	char *duplicated = xstrdup(file);
	char *shortname = strrchr(duplicated, '/');

	DEBUG("get song: %s\n", file);

	if (!shortname) {
		shortname = duplicated;
	} else {
		*shortname = '\0';
		++shortname;
		dir = duplicated;
	}

	if (!(directory = getDirectory(dir)))
		goto out;
	if (!(song = songvec_find(&directory->songs, shortname)))
		goto out;
	assert(song->parentDir == directory);

out:
	free(duplicated);
	return song;
}

time_t getDbModTime(void)
{
	return directory_dbModTime;
}
