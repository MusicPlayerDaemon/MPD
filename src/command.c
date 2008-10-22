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

#include "command.h"
#include "player_control.h"
#include "playlist.h"
#include "ls.h"
#include "directory.h"
#include "directory_print.h"
#include "database.h"
#include "update.h"
#include "volume.h"
#include "stats.h"
#include "permission.h"
#include "buffer2array.h"
#include "log.h"
#include "utils.h"
#include "stored_playlist.h"
#include "sllist.h"
#include "ack.h"
#include "audio.h"
#include "dbUtils.h"
#include "tag.h"
#include "client.h"
#include "tag_print.h"
#include "path.h"
#include "os_compat.h"

#define COMMAND_STATUS_VOLUME           "volume"
#define COMMAND_STATUS_STATE            "state"
#define COMMAND_STATUS_REPEAT           "repeat"
#define COMMAND_STATUS_RANDOM           "random"
#define COMMAND_STATUS_PLAYLIST         "playlist"
#define COMMAND_STATUS_PLAYLIST_LENGTH  "playlistlength"
#define COMMAND_STATUS_SONG             "song"
#define COMMAND_STATUS_SONGID           "songid"
#define COMMAND_STATUS_TIME             "time"
#define COMMAND_STATUS_BITRATE          "bitrate"
#define COMMAND_STATUS_ERROR            "error"
#define COMMAND_STATUS_CROSSFADE	"xfade"
#define COMMAND_STATUS_AUDIO		"audio"
#define COMMAND_STATUS_UPDATING_DB	"updating_db"

/*
 * The most we ever use is for search/find, and that limits it to the
 * number of tags we can have.  Add one for the command, and one extra
 * to catch errors clients may send us
 */
#define COMMAND_ARGV_MAX	(2+(TAG_NUM_OF_ITEM_TYPES*2))

typedef int (*CommandHandlerFunction) (struct client *, int, char **);

/* if min: -1 don't check args *
 * if max: -1 no max args      */
struct command {
	const char *cmd;
	unsigned reqPermission;
	int min;
	int max;
	CommandHandlerFunction handler;
};

/* this should really be "need a non-negative integer": */
static const char need_positive[] = "need a positive integer"; /* no-op */

/* FIXME: redundant error messages */
static const char check_integer[] = "\"%s\" is not a integer";
static const char need_integer[] = "need an integer";
static const char check_boolean[] = "\"%s\" is not 0 or 1";
static const char check_non_negative[] = "\"%s\" is not an integer >= 0";

static const char *current_command;
static int command_listNum;

void command_success(struct client *client)
{
	client_puts(client, "OK\n");
}

static void command_error_v(struct client *client, enum ack error,
			    const char *fmt, va_list args)
{
	assert(client != NULL);
	assert(current_command != NULL);

	client_printf(client, "ACK [%i@%i] {%s} ",
		      (int)error, command_listNum, current_command);
	client_vprintf(client, fmt, args);
	client_puts(client, "\n");

	current_command = NULL;
}

mpd_fprintf_ void command_error(struct client *client, enum ack error,
				const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	command_error_v(client, error, fmt, args);
	va_end(args);
}

static int mpd_fprintf__ check_uint32(struct client *client, uint32_t *dst,
                                      const char *s, const char *fmt, ...)
{
	char *test;

	*dst = strtoul(s, &test, 10);
	if (*test != '\0') {
		va_list args;
		va_start(args, fmt);
		command_error_v(client, ACK_ERROR_ARG, fmt, args);
		va_end(args);
		return -1;
	}
	return 0;
}

static int mpd_fprintf__ check_int(struct client *client, int *dst,
                                   const char *s, const char *fmt, ...)
{
	char *test;

	*dst = strtol(s, &test, 10);
	if (*test != '\0' ||
	    (fmt == check_boolean && *dst != 0 && *dst != 1) ||
	    (fmt == check_non_negative && *dst < 0)) {
		va_list args;
		va_start(args, fmt);
		command_error_v(client, ACK_ERROR_ARG, fmt, args);
		va_end(args);
		return -1;
	}
	return 0;
}

static int print_playlist_result(struct client *client,
				 enum playlist_result result)
{
	switch (result) {
	case PLAYLIST_RESULT_SUCCESS:
		return 0;

	case PLAYLIST_RESULT_ERRNO:
		command_error(client, ACK_ERROR_SYSTEM, strerror(errno));
		return -1;

	case PLAYLIST_RESULT_DENIED:
		command_error(client, ACK_ERROR_NO_EXIST, "Access denied");
		return -1;

	case PLAYLIST_RESULT_NO_SUCH_SONG:
		command_error(client, ACK_ERROR_NO_EXIST, "No such song");
		return -1;

	case PLAYLIST_RESULT_NO_SUCH_LIST:
		command_error(client, ACK_ERROR_NO_EXIST, "No such playlist");
		return -1;

	case PLAYLIST_RESULT_LIST_EXISTS:
		command_error(client, ACK_ERROR_EXIST,
			      "Playlist already exists");
		return -1;

	case PLAYLIST_RESULT_BAD_NAME:
		command_error(client, ACK_ERROR_ARG,
			      "playlist name is invalid: "
			      "playlist names may not contain slashes,"
			      " newlines or carriage returns");
		return -1;

	case PLAYLIST_RESULT_BAD_RANGE:
		command_error(client, ACK_ERROR_ARG, "Bad song index");
		return -1;

	case PLAYLIST_RESULT_NOT_PLAYING:
		command_error(client, ACK_ERROR_PLAYER_SYNC, "Not playing");
		return -1;

	case PLAYLIST_RESULT_TOO_LARGE:
		command_error(client, ACK_ERROR_PLAYLIST_MAX,
			      "playlist is at the max size");
		return -1;
	}

	assert(0);
	return -1;
}

static void
print_spl_list(struct client *client, GPtrArray *list)
{
	for (unsigned i = 0; i < list->len; ++i) {
		struct stored_playlist_info *playlist =
			g_ptr_array_index(list, i);
		time_t t;
		struct tm tm;
		char timestamp[32];

		client_printf(client, "playlist: %s\n", playlist->name);

		t = playlist->mtime;
		strftime(timestamp, sizeof(timestamp), "%FT%TZ",
			 gmtime_r(&t, &tm));
		client_printf(client, "Last-Modified: %s\n", timestamp);
	}
}

static int handleUrlHandlers(struct client *client,
			     mpd_unused int argc, mpd_unused char *argv[])
{
	if (client_get_uid(client) > 0)
		client_puts(client, "handler: file://\n");
	return printRemoteUrlHandlers(client);
}

static int handleTagTypes(struct client *client,
			  mpd_unused int argc, mpd_unused char *argv[])
{
	tag_print_types(client);
	return 0;
}

static int handlePlay(struct client *client,
		      int argc, char *argv[])
{
	int song = -1;
	enum playlist_result result;

	if (argc == 2 && check_int(client, &song, argv[1], need_positive) < 0)
		return -1;
	result = playPlaylist(song, 0);
	return print_playlist_result(client, result);
}

static int handlePlayId(struct client *client,
			int argc, char *argv[])
{
	int id = -1;
	enum playlist_result result;

	if (argc == 2 && check_int(client, &id, argv[1], need_positive) < 0)
		return -1;

	result = playPlaylistById(id, 0);
	return print_playlist_result(client, result);
}

static int handleStop(mpd_unused struct client *client,
		      mpd_unused int argc, mpd_unused char *argv[])
{
	stopPlaylist();
	return 0;
}

static int handleCurrentSong(struct client *client,
			     mpd_unused int argc, mpd_unused char *argv[])
{
	int song = getPlaylistCurrentSong();
	enum playlist_result result;

	if (song < 0)
		return 0;

	result = playlistInfo(client, song);
	return print_playlist_result(client, result);
}

static int handlePause(struct client *client,
		       int argc, char *argv[])
{
	if (argc == 2) {
		int pause_flag;
		if (check_int(client, &pause_flag, argv[1], check_boolean, argv[1]) < 0)
			return -1;
		playerSetPause(pause_flag);
		return 0;
	}

	playerPause();
	return 0;
}

static int commandStatus(struct client *client,
			 mpd_unused int argc, mpd_unused char *argv[])
{
	const char *state = NULL;
	int updateJobId;
	int song;

	playPlaylistIfPlayerStopped();
	switch (getPlayerState()) {
	case PLAYER_STATE_STOP:
		state = "stop";
		break;
	case PLAYER_STATE_PAUSE:
		state = "pause";
		break;
	case PLAYER_STATE_PLAY:
		state = "play";
		break;
	}

	client_printf(client,
		      COMMAND_STATUS_VOLUME ": %i\n"
		      COMMAND_STATUS_REPEAT ": %i\n"
		      COMMAND_STATUS_RANDOM ": %i\n"
		      COMMAND_STATUS_PLAYLIST ": %li\n"
		      COMMAND_STATUS_PLAYLIST_LENGTH ": %i\n"
		      COMMAND_STATUS_CROSSFADE ": %i\n"
		      COMMAND_STATUS_STATE ": %s\n",
		      getVolumeLevel(),
		      getPlaylistRepeatStatus(),
		      getPlaylistRandomStatus(),
		      getPlaylistVersion(),
		      getPlaylistLength(),
		      (int)(getPlayerCrossFade() + 0.5),
		      state);

	song = getPlaylistCurrentSong();
	if (song >= 0) {
		client_printf(client,
			      COMMAND_STATUS_SONG ": %i\n"
			      COMMAND_STATUS_SONGID ": %i\n",
			      song, getPlaylistSongId(song));
	}

	if (getPlayerState() != PLAYER_STATE_STOP) {
		const struct audio_format *af = player_get_audio_format();
		client_printf(client,
			      COMMAND_STATUS_TIME ": %i:%i\n"
			      COMMAND_STATUS_BITRATE ": %li\n"
			      COMMAND_STATUS_AUDIO ": %u:%u:%u\n",
			      getPlayerElapsedTime(), getPlayerTotalTime(),
			      getPlayerBitRate(),
			      af->sample_rate, af->bits, af->channels);
	}

	if ((updateJobId = isUpdatingDB())) {
		client_printf(client,
			      COMMAND_STATUS_UPDATING_DB ": %i\n",
			      updateJobId);
	}

	if (getPlayerError() != PLAYER_ERROR_NOERROR) {
		client_printf(client,
			      COMMAND_STATUS_ERROR ": %s\n",
			      getPlayerErrorStr());
	}

	return 0;
}

static int handleKill(mpd_unused struct client *client,
		      mpd_unused int argc, mpd_unused char *argv[])
{
	return COMMAND_RETURN_KILL;
}

static int handleClose(mpd_unused struct client *client,
		       mpd_unused int argc, mpd_unused char *argv[])
{
	return COMMAND_RETURN_CLOSE;
}

static int handleAdd(struct client *client,
		     mpd_unused int argc, char *argv[])
{
	char *path = argv[1];
	enum playlist_result result;

	if (strncmp(path, "file:///", 8) == 0) {
		result = playlist_append_file(path + 7, client_get_uid(client),
					      NULL);
		return print_playlist_result(client, result);
	}

	if (isRemoteUrl(path))
		return addToPlaylist(path, NULL);

	result = addAllIn(path);
	if (result == (enum playlist_result)-1) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");
		return -1;
	}

	return print_playlist_result(client, result);
}

static int handleAddId(struct client *client,
		       int argc, char *argv[])
{
	int added_id;
	enum playlist_result result;

	if (strncmp(argv[1], "file:///", 8) == 0)
		result = playlist_append_file(argv[1] + 7,
					      client_get_uid(client),
					      &added_id);
	else
		result = addToPlaylist(argv[1], &added_id);

	if (result != PLAYLIST_RESULT_SUCCESS)
		return print_playlist_result(client, result);

	if (argc == 3) {
		int to;
		if (check_int(client, &to, argv[2],
			      check_integer, argv[2]) < 0)
			return -1;
		result = moveSongInPlaylistById(added_id, to);
		if (result != PLAYLIST_RESULT_SUCCESS) {
			int ret = print_playlist_result(client, result);
			deleteFromPlaylistById(added_id);
			return ret;
		}
	}

	client_printf(client, "Id: %d\n", added_id);
	return result;
}

static int handleDelete(struct client *client,
			mpd_unused int argc, char *argv[])
{
	int song;
	enum playlist_result result;

	if (check_int(client, &song, argv[1], need_positive) < 0)
		return -1;

	result = deleteFromPlaylist(song);
	return print_playlist_result(client, result);
}

static int handleDeleteId(struct client *client,
			  mpd_unused int argc, char *argv[])
{
	int id;
	enum playlist_result result;

	if (check_int(client, &id, argv[1], need_positive) < 0)
		return -1;

	result = deleteFromPlaylistById(id);
	return print_playlist_result(client, result);
}

static int handlePlaylist(struct client *client,
			  mpd_unused int argc, mpd_unused char *argv[])
{
	showPlaylist(client);
	return 0;
}

static int handleShuffle(mpd_unused struct client *client,
			 mpd_unused int argc, mpd_unused char *argv[])
{
	shufflePlaylist();
	return 0;
}

static int handleClear(mpd_unused struct client *client,
		       mpd_unused int argc, mpd_unused char *argv[])
{
	clearPlaylist();
	return 0;
}

static int handleSave(struct client *client,
		      mpd_unused int argc, char *argv[])
{
	enum playlist_result result;

	result = savePlaylist(argv[1]);
	return print_playlist_result(client, result);
}

static int handleLoad(struct client *client,
		      mpd_unused int argc, char *argv[])
{
	enum playlist_result result;

	result = loadPlaylist(client, argv[1]);
	return print_playlist_result(client, result);
}

static int handleListPlaylist(struct client *client,
			      mpd_unused int argc, char *argv[])
{
	int ret;

	ret = PlaylistInfo(client, argv[1], 0);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST, "No such playlist");

	return ret;
}

static int handleListPlaylistInfo(struct client *client,
				  mpd_unused int argc, char *argv[])
{
	int ret;

	ret = PlaylistInfo(client, argv[1], 1);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST, "No such playlist");

	return ret;
}

static int handleLsInfo(struct client *client,
			int argc, char *argv[])
{
	const char *path = "";
	const struct directory *directory;

	if (argc == 2)
		path = argv[1];

	directory = db_get_directory(path);
	if (directory == NULL) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory not found");
		return -1;
	}

	directory_print(client, directory);

	if (isRootDirectory(path)) {
		GPtrArray *list = spl_list();
		if (list != NULL) {
			print_spl_list(client, list);
			spl_list_free(list);
		}
	}

	return 0;
}

static int handleRm(struct client *client,
		    mpd_unused int argc, char *argv[])
{
	enum playlist_result result;

	result = deletePlaylist(argv[1]);
	return print_playlist_result(client, result);
}

static int handleRename(struct client *client,
			mpd_unused int argc, char *argv[])
{
	enum playlist_result result;

	result = spl_rename(argv[1], argv[2]);
	return print_playlist_result(client, result);
}

static int handlePlaylistChanges(struct client *client,
				 mpd_unused int argc, char *argv[])
{
	uint32_t version;

	if (check_uint32(client, &version, argv[1], need_positive) < 0)
		return -1;
	return playlistChanges(client, version);
}

static int handlePlaylistChangesPosId(struct client *client,
				      mpd_unused int argc, char *argv[])
{
	uint32_t version;

	if (check_uint32(client, &version, argv[1], need_positive) < 0)
		return -1;
	return playlistChangesPosId(client, version);
}

static int handlePlaylistInfo(struct client *client,
			      int argc, char *argv[])
{
	int song = -1;
	enum playlist_result result;

	if (argc == 2 && check_int(client, &song, argv[1], need_positive) < 0)
		return -1;

	result = playlistInfo(client, song);
	return print_playlist_result(client, result);
}

static int handlePlaylistId(struct client *client,
			    int argc, char *argv[])
{
	int id = -1;
	enum playlist_result result;

	if (argc == 2 && check_int(client, &id, argv[1], need_positive) < 0)
		return -1;

	result = playlistId(client, id);
	return print_playlist_result(client, result);
}

static int handleFind(struct client *client,
		      int argc, char *argv[])
{
	int ret;

	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return -1;
	}

	ret = findSongsIn(client, NULL, numItems, items);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");

	freeLocateTagItemArray(numItems, items);

	return ret;
}

static int handleSearch(struct client *client,
			int argc, char *argv[])
{
	int ret;

	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return -1;
	}

	ret = searchForSongsIn(client, NULL, numItems, items);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");

	freeLocateTagItemArray(numItems, items);

	return ret;
}

static int handleCount(struct client *client,
		       int argc, char *argv[])
{
	int ret;

	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return -1;
	}

	ret = searchStatsForSongsIn(client, NULL, numItems, items);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");

	freeLocateTagItemArray(numItems, items);

	return ret;
}

static int handlePlaylistFind(struct client *client,
			      int argc, char *argv[])
{
	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return -1;
	}

	findSongsInPlaylist(client, numItems, items);

	freeLocateTagItemArray(numItems, items);

	return 0;
}

static int handlePlaylistSearch(struct client *client,
				int argc, char *argv[])
{
	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return -1;
	}

	searchForSongsInPlaylist(client, numItems, items);

	freeLocateTagItemArray(numItems, items);

	return 0;
}

static int handlePlaylistDelete(struct client *client,
				mpd_unused int argc, char *argv[]) {
	char *playlist = argv[1];
	int from;
	enum playlist_result result;

	if (check_int(client, &from, argv[2], check_integer, argv[2]) < 0)
		return -1;

	result = spl_remove_index(playlist, from);
	return print_playlist_result(client, result);
}

static int handlePlaylistMove(struct client *client,
			      mpd_unused mpd_unused int argc, char *argv[])
{
	char *playlist = argv[1];
	int from, to;
	enum playlist_result result;

	if (check_int(client, &from, argv[2], check_integer, argv[2]) < 0)
		return -1;
	if (check_int(client, &to, argv[3], check_integer, argv[3]) < 0)
		return -1;

	result = spl_move_index(playlist, from, to);
	return print_playlist_result(client, result);
}

static int handleUpdate(struct client *client,
			mpd_unused int argc, char *argv[])
{
	char *path = NULL;
	unsigned ret;

	assert(argc <= 2);
	if (argc == 2 && !(path = sanitizePathDup(argv[1]))) {
		command_error(client, ACK_ERROR_ARG, "invalid path");
		return -1;
	}

	ret = directory_update_init(path);
	if (ret > 0) {
		client_printf(client, "updating_db: %i\n", ret);
		return 0;
	} else {
		command_error(client, ACK_ERROR_UPDATE_ALREADY,
			      "already updating");
		return -1;
	}
}

static int handleNext(mpd_unused struct client *client,
		      mpd_unused int argc, mpd_unused char *argv[])
{
	nextSongInPlaylist();
	return 0;
}

static int handlePrevious(mpd_unused struct client *client,
			  mpd_unused int argc, mpd_unused char *argv[])
{
	previousSongInPlaylist();
	return 0;
}

static int handleListAll(struct client *client,
			 mpd_unused int argc, char *argv[])
{
	char *directory = NULL;
	int ret;

	if (argc == 2)
		directory = argv[1];

	ret = printAllIn(client, directory);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");

	return ret;
}

static int handleVolume(struct client *client,
			mpd_unused int argc, char *argv[])
{
	int change, ret;

	if (check_int(client, &change, argv[1], need_integer) < 0)
		return -1;

	ret = changeVolumeLevel(change, 1);
	if (ret == -1)
		command_error(client, ACK_ERROR_SYSTEM,
			      "problems setting volume");

	return ret;
}

static int handleSetVol(struct client *client,
			mpd_unused int argc, char *argv[])
{
	int level, ret;

	if (check_int(client, &level, argv[1], need_integer) < 0)
		return -1;

	ret = changeVolumeLevel(level, 0);
	if (ret == -1)
		command_error(client, ACK_ERROR_SYSTEM,
			      "problems setting volume");

	return ret;
}

static int handleRepeat(struct client *client,
			mpd_unused int argc, char *argv[])
{
	int status;

	if (check_int(client, &status, argv[1], need_integer) < 0)
		return -1;

	if (status != 0 && status != 1) {
		command_error(client, ACK_ERROR_ARG,
			      "\"%i\" is not 0 or 1", status);
		return -1;
	}

	setPlaylistRepeatStatus(status);
	return 0;
}

static int handleRandom(struct client *client,
			mpd_unused int argc, char *argv[])
{
	int status;

	if (check_int(client, &status, argv[1], need_integer) < 0)
		return -1;

	if (status != 0 && status != 1) {
		command_error(client, ACK_ERROR_ARG,
			      "\"%i\" is not 0 or 1", status);
		return -1;
	}

	setPlaylistRandomStatus(status);
	return 0;
}

static int handleStats(struct client *client,
		       mpd_unused int argc, mpd_unused char *argv[])
{
	return printStats(client);
}

static int handleClearError(mpd_unused struct client *client,
			    mpd_unused int argc, mpd_unused char *argv[])
{
	clearPlayerError();
	return 0;
}

static int handleList(struct client *client,
		      int argc, char *argv[])
{
	int numConditionals;
	LocateTagItem *conditionals = NULL;
	int tagType = getLocateTagItemType(argv[1]);
	int ret;

	if (tagType < 0) {
		command_error(client, ACK_ERROR_ARG, "\"%s\" is not known", argv[1]);
		return -1;
	}

	if (tagType == LOCATE_TAG_ANY_TYPE) {
		command_error(client, ACK_ERROR_ARG,
			      "\"any\" is not a valid return tag type");
		return -1;
	}

	/* for compatibility with < 0.12.0 */
	if (argc == 3) {
		if (tagType != TAG_ITEM_ALBUM) {
			command_error(client, ACK_ERROR_ARG,
				      "should be \"%s\" for 3 arguments",
				      mpdTagItemKeys[TAG_ITEM_ALBUM]);
			return -1;
		}
		conditionals = newLocateTagItem(mpdTagItemKeys[TAG_ITEM_ARTIST],
						argv[2]);
		numConditionals = 1;
	} else {
		numConditionals =
		    newLocateTagItemArrayFromArgArray(argv + 2,
						      argc - 2, &conditionals);

		if (numConditionals < 0) {
			command_error(client, ACK_ERROR_ARG,
				      "not able to parse args");
			return -1;
		}
	}

	ret = listAllUniqueTags(client, tagType, numConditionals, conditionals);

	if (conditionals)
		freeLocateTagItemArray(numConditionals, conditionals);

	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");

	return ret;
}

static int handleMove(struct client *client,
		      mpd_unused int argc, char *argv[])
{
	int from, to;
	enum playlist_result result;

	if (check_int(client, &from, argv[1], check_integer, argv[1]) < 0)
		return -1;
	if (check_int(client, &to, argv[2], check_integer, argv[2]) < 0)
		return -1;
	result = moveSongInPlaylist(from, to);
	return print_playlist_result(client, result);
}

static int handleMoveId(struct client *client,
			mpd_unused int argc, char *argv[])
{
	int id, to;
	enum playlist_result result;

	if (check_int(client, &id, argv[1], check_integer, argv[1]) < 0)
		return -1;
	if (check_int(client, &to, argv[2], check_integer, argv[2]) < 0)
		return -1;
	result = moveSongInPlaylistById(id, to);
	return print_playlist_result(client, result);
}

static int handleSwap(struct client *client,
		      mpd_unused int argc, char *argv[])
{
	int song1, song2;
	enum playlist_result result;

	if (check_int(client, &song1, argv[1], check_integer, argv[1]) < 0)
		return -1;
	if (check_int(client, &song2, argv[2], check_integer, argv[2]) < 0)
		return -1;
	result = swapSongsInPlaylist(song1, song2);
	return print_playlist_result(client, result);
}

static int handleSwapId(struct client *client,
			mpd_unused int argc, char *argv[])
{
	int id1, id2;
	enum playlist_result result;

	if (check_int(client, &id1, argv[1], check_integer, argv[1]) < 0)
		return -1;
	if (check_int(client, &id2, argv[2], check_integer, argv[2]) < 0)
		return -1;
	result = swapSongsInPlaylistById(id1, id2);
	return print_playlist_result(client, result);
}

static int handleSeek(struct client *client,
		      mpd_unused int argc, char *argv[])
{
	int song, seek_time;
	enum playlist_result result;

	if (check_int(client, &song, argv[1], check_integer, argv[1]) < 0)
		return -1;
	if (check_int(client, &seek_time, argv[2], check_integer, argv[2]) < 0)
		return -1;

	result = seekSongInPlaylist(song, seek_time);
	return print_playlist_result(client, result);
}

static int handleSeekId(struct client *client,
			mpd_unused int argc, char *argv[])
{
	int id, seek_time;
	enum playlist_result result;

	if (check_int(client, &id, argv[1], check_integer, argv[1]) < 0)
		return -1;
	if (check_int(client, &seek_time, argv[2], check_integer, argv[2]) < 0)
		return -1;

	result = seekSongInPlaylistById(id, seek_time);
	return print_playlist_result(client, result);
}

static int handleListAllInfo(struct client *client,
			     mpd_unused int argc, char *argv[])
{
	char *directory = NULL;
	int ret;

	if (argc == 2)
		directory = argv[1];

	ret = printInfoForAllIn(client, directory);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");

	return ret;
}

static int handlePing(mpd_unused struct client *client,
		      mpd_unused int argc, mpd_unused char *argv[])
{
	return 0;
}

static int handlePassword(struct client *client,
			  mpd_unused int argc, char *argv[])
{
	unsigned permission = 0;

	if (getPermissionFromPassword(argv[1], &permission) < 0) {
		command_error(client, ACK_ERROR_PASSWORD, "incorrect password");
		return -1;
	}

	client_set_permission(client, permission);

	return 0;
}

static int handleCrossfade(struct client *client,
			   mpd_unused int argc, char *argv[])
{
	int xfade_time;

	if (check_int(client, &xfade_time, argv[1], check_non_negative, argv[1]) < 0)
		return -1;
	setPlayerCrossFade(xfade_time);

	return 0;
}

static int handleEnableDevice(struct client *client,
			      mpd_unused int argc, char *argv[])
{
	int device, ret;

	if (check_int(client, &device, argv[1], check_non_negative, argv[1]) < 0)
		return -1;

	ret = enableAudioDevice(device);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "No such audio output");

	return ret;
}

static int handleDisableDevice(struct client *client,
			       mpd_unused int argc, char *argv[])
{
	int device, ret;

	if (check_int(client, &device, argv[1], check_non_negative, argv[1]) < 0)
		return -1;

	ret = disableAudioDevice(device);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "No such audio output");

	return ret;
}

static int handleDevices(struct client *client,
			 mpd_unused int argc, mpd_unused char *argv[])
{
	printAudioDevices(client);

	return 0;
}

/* don't be fooled, this is the command handler for "commands" command */
static int handleCommands(struct client *client,
			  mpd_unused int argc, mpd_unused char *argv[]);

static int handleNotcommands(struct client *client,
			     mpd_unused int argc, mpd_unused char *argv[]);

static int handlePlaylistClear(struct client *client,
			       mpd_unused int argc, char *argv[])
{
	enum playlist_result result;

	result = spl_clear(argv[1]);
	return print_playlist_result(client, result);
}

static int handlePlaylistAdd(struct client *client,
			     mpd_unused int argc, char *argv[])
{
	char *playlist = argv[1];
	char *path = argv[2];
	enum playlist_result result;

	if (isRemoteUrl(path))
		result = spl_append_uri(path, playlist);
	else
		result = addAllInToStoredPlaylist(path, playlist);

	if (result == (enum playlist_result)-1) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");
		return -1;
	}

	return print_playlist_result(client, result);
}

static int
handle_list_playlists(struct client *client,
		      mpd_unused int argc, mpd_unused char *argv[])
{
	GPtrArray *list = spl_list();
	if (list == NULL) {
		command_error(client, ACK_ERROR_SYSTEM,
			      "failed to get list of stored playlists");
		return -1;
	}

	print_spl_list(client, list);
	spl_list_free(list);
	return 0;
}

static int
handle_idle(struct client *client,
	    mpd_unused int argc, mpd_unused char *argv[])
{
	/* enable "idle" mode on this client */
	client_idle_wait(client);

	/* return value is "1" so the caller won't print "OK" */
	return 1;
}

/**
 * The command registry.
 *
 * This array must be sorted!
 */
static const struct command commands[] = {
	{ "add", PERMISSION_ADD, 1, 1, handleAdd },
	{ "addid", PERMISSION_ADD, 1, 2, handleAddId },
	{ "clear", PERMISSION_CONTROL, 0, 0, handleClear },
	{ "clearerror", PERMISSION_CONTROL, 0, 0, handleClearError },
	{ "close", PERMISSION_NONE, -1, -1, handleClose },
	{ "commands", PERMISSION_NONE, 0, 0, handleCommands },
	{ "count", PERMISSION_READ, 2, -1, handleCount },
	{ "crossfade", PERMISSION_CONTROL, 1, 1, handleCrossfade },
	{ "currentsong", PERMISSION_READ, 0, 0, handleCurrentSong },
	{ "delete", PERMISSION_CONTROL, 1, 1, handleDelete },
	{ "deleteid", PERMISSION_CONTROL, 1, 1, handleDeleteId },
	{ "disableoutput", PERMISSION_ADMIN, 1, 1, handleDisableDevice },
	{ "enableoutput", PERMISSION_ADMIN, 1, 1, handleEnableDevice },
	{ "find", PERMISSION_READ, 2, -1, handleFind },
	{ "idle", PERMISSION_READ, 0, 0, handle_idle },
	{ "kill", PERMISSION_ADMIN, -1, -1, handleKill },
	{ "list", PERMISSION_READ, 1, -1, handleList },
	{ "listall", PERMISSION_READ, 0, 1, handleListAll },
	{ "listallinfo", PERMISSION_READ, 0, 1, handleListAllInfo },
	{ "listplaylist", PERMISSION_READ, 1, 1, handleListPlaylist },
	{ "listplaylistinfo", PERMISSION_READ, 1, 1, handleListPlaylistInfo },
	{ "listplaylists", PERMISSION_READ, 0, 0, handle_list_playlists },
	{ "load", PERMISSION_ADD, 1, 1, handleLoad },
	{ "lsinfo", PERMISSION_READ, 0, 1, handleLsInfo },
	{ "move", PERMISSION_CONTROL, 2, 2, handleMove },
	{ "moveid", PERMISSION_CONTROL, 2, 2, handleMoveId },
	{ "next", PERMISSION_CONTROL, 0, 0, handleNext },
	{ "notcommands", PERMISSION_NONE, 0, 0, handleNotcommands },
	{ "outputs", PERMISSION_READ, 0, 0, handleDevices },
	{ "password", PERMISSION_NONE, 1, 1, handlePassword },
	{ "pause", PERMISSION_CONTROL, 0, 1, handlePause },
	{ "ping", PERMISSION_NONE, 0, 0, handlePing },
	{ "play", PERMISSION_CONTROL, 0, 1, handlePlay },
	{ "playid", PERMISSION_CONTROL, 0, 1, handlePlayId },
	{ "playlist", PERMISSION_READ, 0, 0, handlePlaylist },
	{ "playlistadd", PERMISSION_CONTROL, 2, 2, handlePlaylistAdd },
	{ "playlistclear", PERMISSION_CONTROL, 1, 1, handlePlaylistClear },
	{ "playlistdelete", PERMISSION_CONTROL, 2, 2, handlePlaylistDelete },
	{ "playlistfind", PERMISSION_READ, 2, -1, handlePlaylistFind },
	{ "playlistid", PERMISSION_READ, 0, 1, handlePlaylistId },
	{ "playlistinfo", PERMISSION_READ, 0, 1, handlePlaylistInfo },
	{ "playlistmove", PERMISSION_CONTROL, 3, 3, handlePlaylistMove },
	{ "playlistsearch", PERMISSION_READ, 2, -1, handlePlaylistSearch },
	{ "plchanges", PERMISSION_READ, 1, 1, handlePlaylistChanges },
	{ "plchangesposid", PERMISSION_READ, 1, 1,
	  handlePlaylistChangesPosId },
	{ "previous", PERMISSION_CONTROL, 0, 0, handlePrevious },
	{ "random", PERMISSION_CONTROL, 1, 1, handleRandom },
	{ "rename", PERMISSION_CONTROL, 2, 2, handleRename },
	{ "repeat", PERMISSION_CONTROL, 1, 1, handleRepeat },
	{ "rm", PERMISSION_CONTROL, 1, 1, handleRm },
	{ "save", PERMISSION_CONTROL, 1, 1, handleSave },
	{ "search", PERMISSION_READ, 2, -1, handleSearch },
	{ "seek", PERMISSION_CONTROL, 2, 2, handleSeek },
	{ "seekid", PERMISSION_CONTROL, 2, 2, handleSeekId },
	{ "setvol", PERMISSION_CONTROL, 1, 1, handleSetVol },
	{ "shuffle", PERMISSION_CONTROL, 0, 0, handleShuffle },
	{ "stats", PERMISSION_READ, 0, 0, handleStats },
	{ "status", PERMISSION_READ, 0, 0, commandStatus },
	{ "stop", PERMISSION_CONTROL, 0, 0, handleStop },
	{ "swap", PERMISSION_CONTROL, 2, 2, handleSwap },
	{ "swapid", PERMISSION_CONTROL, 2, 2, handleSwapId },
	{ "tagtypes", PERMISSION_READ, 0, 0, handleTagTypes },
	{ "update", PERMISSION_ADMIN, 0, 1, handleUpdate },
	{ "urlhandlers", PERMISSION_READ, 0, 0, handleUrlHandlers },
	{ "volume", PERMISSION_CONTROL, 1, 1, handleVolume },
};

static const unsigned num_commands = sizeof(commands) / sizeof(commands[0]);

/* don't be fooled, this is the command handler for "commands" command */
static int handleCommands(struct client *client,
			  mpd_unused int argc, mpd_unused char *argv[])
{
	const unsigned permission = client_get_permission(client);
	const struct command *cmd;

	for (unsigned i = 0; i < num_commands; ++i) {
		cmd = &commands[i];

		if (cmd->reqPermission == (permission & cmd->reqPermission)) {
			client_printf(client, "command: %s\n", cmd->cmd);
		}
	}

	return 0;
}

static int handleNotcommands(struct client *client,
			     mpd_unused int argc, mpd_unused char *argv[])
{
	const unsigned permission = client_get_permission(client);
	const struct command *cmd;

	for (unsigned i = 0; i < num_commands; ++i) {
		cmd = &commands[i];

		if (cmd->reqPermission != (permission & cmd->reqPermission)) {
			client_printf(client, "command: %s\n", cmd->cmd);
		}
	}

	return 0;
}

void initCommands(void)
{
#ifndef NDEBUG
	/* ensure that the command list is sorted */
	for (unsigned i = 0; i < num_commands - 1; ++i)
		assert(strcmp(commands[i].cmd, commands[i + 1].cmd) < 0);
#endif
}

void finishCommands(void)
{
}

static const struct command *
command_lookup(const char *name)
{
	unsigned a = 0, b = num_commands, i;
	int cmp;

	/* binary search */
	do {
		i = (a + b) / 2;

		cmp = strcmp(name, commands[i].cmd);
		if (cmp == 0)
			return &commands[i];
		else if (cmp < 0)
			b = i;
		else if (cmp > 0)
			a = i + 1;
	} while (a < b);

	return NULL;
}

static int
checkArgcAndPermission(const struct command *cmd, struct client *client,
		       unsigned permission, int argc, char *argv[])
{
	int min = cmd->min + 1;
	int max = cmd->max + 1;

	if (cmd->reqPermission != (permission & cmd->reqPermission)) {
		if (client != NULL)
			command_error(client, ACK_ERROR_PERMISSION,
				      "you don't have permission for \"%s\"",
				      cmd->cmd);
		return -1;
	}

	if (min == 0)
		return 0;

	if (min == max && max != argc) {
		if (client != NULL)
			command_error(client, ACK_ERROR_ARG,
				      "wrong number of arguments for \"%s\"",
				      argv[0]);
		return -1;
	} else if (argc < min) {
		if (client != NULL)
			command_error(client, ACK_ERROR_ARG,
				      "too few arguments for \"%s\"", argv[0]);
		return -1;
	} else if (argc > max && max /* != 0 */ ) {
		if (client != NULL)
			command_error(client, ACK_ERROR_ARG,
				      "too many arguments for \"%s\"", argv[0]);
		return -1;
	} else
		return 0;
}

static const struct command *
getCommandEntryAndCheckArgcAndPermission(struct client *client,
					 unsigned permission,
					 int argc, char *argv[])
{
	static char unknown[] = "";
	const struct command *cmd;

	current_command = unknown;

	if (argc == 0)
		return NULL;

	cmd = command_lookup(argv[0]);
	if (cmd == NULL) {
		if (client != NULL)
			command_error(client, ACK_ERROR_UNKNOWN,
				      "unknown command \"%s\"", argv[0]);
		return NULL;
	}

	current_command = cmd->cmd;

	if (checkArgcAndPermission(cmd, client, permission, argc, argv) < 0) {
		return NULL;
	}

	return cmd;
}

int processCommand(struct client *client, char *commandString)
{
	int argc;
	char *argv[COMMAND_ARGV_MAX] = { NULL };
	const struct command *cmd;
	int ret = -1;

	if (!(argc = buffer2array(commandString, argv, COMMAND_ARGV_MAX)))
		return 0;

	cmd = getCommandEntryAndCheckArgcAndPermission(client,
						       client_get_permission(client),
						       argc, argv);
	if (cmd)
		ret = cmd->handler(client, argc, argv);

	current_command = NULL;

	return ret;
}

int processListOfCommands(struct client *client,
			  int listOK, struct strnode *list)
{
	struct strnode *cur = list;
	int ret = 0;

	command_listNum = 0;

	while (cur) {
		DEBUG("processListOfCommands: process command \"%s\"\n",
		      cur->data);
		ret = processCommand(client, cur->data);
		DEBUG("processListOfCommands: command returned %i\n", ret);
		if (ret != 0 || client_is_expired(client))
			goto out;
		else if (listOK)
			client_puts(client, "list_OK\n");
		command_listNum++;
		cur = cur->next;
	}
out:
	command_listNum = 0;
	return ret;
}
