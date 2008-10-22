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

/* if min: -1 don't check args *
 * if max: -1 no max args      */
struct command {
	const char *cmd;
	unsigned permission;
	int min;
	int max;
	enum command_return (*handler)(struct client *client, int argc, char **argv);
};

/* this should really be "need a non-negative integer": */
static const char need_positive[] = "need a positive integer"; /* no-op */

/* FIXME: redundant error messages */
static const char check_integer[] = "\"%s\" is not a integer";
static const char need_integer[] = "need an integer";
static const char check_boolean[] = "\"%s\" is not 0 or 1";
static const char check_non_negative[] = "\"%s\" is not an integer >= 0";

static const char *current_command;
static int command_list_num;

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
		      (int)error, command_list_num, current_command);
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

static enum command_return
print_playlist_result(struct client *client,
		      enum playlist_result result)
{
	switch (result) {
	case PLAYLIST_RESULT_SUCCESS:
		return COMMAND_RETURN_OK;

	case PLAYLIST_RESULT_ERRNO:
		command_error(client, ACK_ERROR_SYSTEM, strerror(errno));
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_DENIED:
		command_error(client, ACK_ERROR_NO_EXIST, "Access denied");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_NO_SUCH_SONG:
		command_error(client, ACK_ERROR_NO_EXIST, "No such song");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_NO_SUCH_LIST:
		command_error(client, ACK_ERROR_NO_EXIST, "No such playlist");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_LIST_EXISTS:
		command_error(client, ACK_ERROR_EXIST,
			      "Playlist already exists");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_BAD_NAME:
		command_error(client, ACK_ERROR_ARG,
			      "playlist name is invalid: "
			      "playlist names may not contain slashes,"
			      " newlines or carriage returns");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_BAD_RANGE:
		command_error(client, ACK_ERROR_ARG, "Bad song index");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_NOT_PLAYING:
		command_error(client, ACK_ERROR_PLAYER_SYNC, "Not playing");
		return COMMAND_RETURN_ERROR;

	case PLAYLIST_RESULT_TOO_LARGE:
		command_error(client, ACK_ERROR_PLAYLIST_MAX,
			      "playlist is at the max size");
		return COMMAND_RETURN_ERROR;
	}

	assert(0);
	return COMMAND_RETURN_ERROR;
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

static enum command_return
handle_urlhandlers(struct client *client,
		   mpd_unused int argc, mpd_unused char *argv[])
{
	if (client_get_uid(client) > 0)
		client_puts(client, "handler: file://\n");
	return printRemoteUrlHandlers(client);
}

static enum command_return
handle_tagtypes(struct client *client,
		mpd_unused int argc, mpd_unused char *argv[])
{
	tag_print_types(client);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_play(struct client *client, int argc, char *argv[])
{
	int song = -1;
	enum playlist_result result;

	if (argc == 2 && check_int(client, &song, argv[1], need_positive) < 0)
		return COMMAND_RETURN_ERROR;
	result = playPlaylist(song, 0);
	return print_playlist_result(client, result);
}

static enum command_return
handle_playid(struct client *client, int argc, char *argv[])
{
	int id = -1;
	enum playlist_result result;

	if (argc == 2 && check_int(client, &id, argv[1], need_positive) < 0)
		return COMMAND_RETURN_ERROR;

	result = playPlaylistById(id, 0);
	return print_playlist_result(client, result);
}

static enum command_return
handle_stop(mpd_unused struct client *client,
	    mpd_unused int argc, mpd_unused char *argv[])
{
	stopPlaylist();
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_currentsong(struct client *client,
		   mpd_unused int argc, mpd_unused char *argv[])
{
	int song = getPlaylistCurrentSong();
	enum playlist_result result;

	if (song < 0)
		return COMMAND_RETURN_OK;

	result = playlistInfo(client, song);
	return print_playlist_result(client, result);
}

static enum command_return
handle_pause(struct client *client,
	     int argc, char *argv[])
{
	if (argc == 2) {
		int pause_flag;
		if (check_int(client, &pause_flag, argv[1], check_boolean, argv[1]) < 0)
			return COMMAND_RETURN_ERROR;
		playerSetPause(pause_flag);
		return COMMAND_RETURN_OK;
	}

	playerPause();
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_status(struct client *client,
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

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_kill(mpd_unused struct client *client,
	    mpd_unused int argc, mpd_unused char *argv[])
{
	return COMMAND_RETURN_KILL;
}

static enum command_return
handle_close(mpd_unused struct client *client,
	     mpd_unused int argc, mpd_unused char *argv[])
{
	return COMMAND_RETURN_CLOSE;
}

static enum command_return
handle_add(struct client *client, mpd_unused int argc, char *argv[])
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
		return COMMAND_RETURN_ERROR;
	}

	return print_playlist_result(client, result);
}

static enum command_return
handle_addid(struct client *client, int argc, char *argv[])
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
			return COMMAND_RETURN_ERROR;
		result = moveSongInPlaylistById(added_id, to);
		if (result != PLAYLIST_RESULT_SUCCESS) {
			enum command_return ret =
				print_playlist_result(client, result);
			deleteFromPlaylistById(added_id);
			return ret;
		}
	}

	client_printf(client, "Id: %d\n", added_id);
	return result;
}

static enum command_return
handle_delete(struct client *client, mpd_unused int argc, char *argv[])
{
	int song;
	enum playlist_result result;

	if (check_int(client, &song, argv[1], need_positive) < 0)
		return COMMAND_RETURN_ERROR;

	result = deleteFromPlaylist(song);
	return print_playlist_result(client, result);
}

static enum command_return
handle_deleteid(struct client *client, mpd_unused int argc, char *argv[])
{
	int id;
	enum playlist_result result;

	if (check_int(client, &id, argv[1], need_positive) < 0)
		return COMMAND_RETURN_ERROR;

	result = deleteFromPlaylistById(id);
	return print_playlist_result(client, result);
}

static enum command_return
handle_playlist(struct client *client,
	       mpd_unused int argc, mpd_unused char *argv[])
{
	showPlaylist(client);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_shuffle(mpd_unused struct client *client,
	       mpd_unused int argc, mpd_unused char *argv[])
{
	shufflePlaylist();
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_clear(mpd_unused struct client *client,
	     mpd_unused int argc, mpd_unused char *argv[])
{
	clearPlaylist();
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_save(struct client *client,
	    mpd_unused int argc, char *argv[])
{
	enum playlist_result result;

	result = savePlaylist(argv[1]);
	return print_playlist_result(client, result);
}

static enum command_return
handle_load(struct client *client, mpd_unused int argc, char *argv[])
{
	enum playlist_result result;

	result = loadPlaylist(client, argv[1]);
	return print_playlist_result(client, result);
}

static enum command_return
handle_listplaylist(struct client *client, mpd_unused int argc, char *argv[])
{
	int ret;

	ret = PlaylistInfo(client, argv[1], 0);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST, "No such playlist");

	return ret;
}

static enum command_return
handle_listplaylistinfo(struct client *client,
			mpd_unused int argc, char *argv[])
{
	int ret;

	ret = PlaylistInfo(client, argv[1], 1);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST, "No such playlist");

	return ret;
}

static enum command_return
handle_lsinfo(struct client *client, int argc, char *argv[])
{
	const char *path = "";
	const struct directory *directory;

	if (argc == 2)
		path = argv[1];

	directory = db_get_directory(path);
	if (directory == NULL) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory not found");
		return COMMAND_RETURN_ERROR;
	}

	directory_print(client, directory);

	if (isRootDirectory(path)) {
		GPtrArray *list = spl_list();
		if (list != NULL) {
			print_spl_list(client, list);
			spl_list_free(list);
		}
	}

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_rm(struct client *client, mpd_unused int argc, char *argv[])
{
	enum playlist_result result;

	result = deletePlaylist(argv[1]);
	return print_playlist_result(client, result);
}

static enum command_return
handle_rename(struct client *client, mpd_unused int argc, char *argv[])
{
	enum playlist_result result;

	result = spl_rename(argv[1], argv[2]);
	return print_playlist_result(client, result);
}

static enum command_return
handle_plchanges(struct client *client, mpd_unused int argc, char *argv[])
{
	uint32_t version;

	if (check_uint32(client, &version, argv[1], need_positive) < 0)
		return COMMAND_RETURN_ERROR;
	return playlistChanges(client, version);
}

static enum command_return
handle_plchangesposid(struct client *client, mpd_unused int argc, char *argv[])
{
	uint32_t version;

	if (check_uint32(client, &version, argv[1], need_positive) < 0)
		return COMMAND_RETURN_ERROR;
	return playlistChangesPosId(client, version);
}

static enum command_return
handle_playlistinfo(struct client *client, int argc, char *argv[])
{
	int song = -1;
	enum playlist_result result;

	if (argc == 2 && check_int(client, &song, argv[1], need_positive) < 0)
		return COMMAND_RETURN_ERROR;

	result = playlistInfo(client, song);
	return print_playlist_result(client, result);
}

static enum command_return
handle_playlistid(struct client *client, int argc, char *argv[])
{
	int id = -1;
	enum playlist_result result;

	if (argc == 2 && check_int(client, &id, argv[1], need_positive) < 0)
		return COMMAND_RETURN_ERROR;

	result = playlistId(client, id);
	return print_playlist_result(client, result);
}

static enum command_return
handle_find(struct client *client, int argc, char *argv[])
{
	int ret;

	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	ret = findSongsIn(client, NULL, numItems, items);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");

	freeLocateTagItemArray(numItems, items);

	return ret;
}

static enum command_return
handle_search(struct client *client, int argc, char *argv[])
{
	int ret;

	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	ret = searchForSongsIn(client, NULL, numItems, items);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");

	freeLocateTagItemArray(numItems, items);

	return ret;
}

static enum command_return
handle_count(struct client *client, int argc, char *argv[])
{
	int ret;

	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	ret = searchStatsForSongsIn(client, NULL, numItems, items);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");

	freeLocateTagItemArray(numItems, items);

	return ret;
}

static enum command_return
handle_playlistfind(struct client *client, int argc, char *argv[])
{
	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	findSongsInPlaylist(client, numItems, items);

	freeLocateTagItemArray(numItems, items);

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_playlistsearch(struct client *client, int argc, char *argv[])
{
	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	searchForSongsInPlaylist(client, numItems, items);

	freeLocateTagItemArray(numItems, items);

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_playlistdelete(struct client *client,
		      mpd_unused int argc, char *argv[]) {
	char *playlist = argv[1];
	int from;
	enum playlist_result result;

	if (check_int(client, &from, argv[2], check_integer, argv[2]) < 0)
		return COMMAND_RETURN_ERROR;

	result = spl_remove_index(playlist, from);
	return print_playlist_result(client, result);
}

static enum command_return
handle_playlistmove(struct client *client, mpd_unused int argc, char *argv[])
{
	char *playlist = argv[1];
	int from, to;
	enum playlist_result result;

	if (check_int(client, &from, argv[2], check_integer, argv[2]) < 0)
		return COMMAND_RETURN_ERROR;
	if (check_int(client, &to, argv[3], check_integer, argv[3]) < 0)
		return COMMAND_RETURN_ERROR;

	result = spl_move_index(playlist, from, to);
	return print_playlist_result(client, result);
}

static enum command_return
handle_update(struct client *client, mpd_unused int argc, char *argv[])
{
	char *path = NULL;
	unsigned ret;

	assert(argc <= 2);
	if (argc == 2 && !(path = sanitizePathDup(argv[1]))) {
		command_error(client, ACK_ERROR_ARG, "invalid path");
		return COMMAND_RETURN_ERROR;
	}

	ret = directory_update_init(path);
	if (ret > 0) {
		client_printf(client, "updating_db: %i\n", ret);
		return COMMAND_RETURN_OK;
	} else {
		command_error(client, ACK_ERROR_UPDATE_ALREADY,
			      "already updating");
		return COMMAND_RETURN_ERROR;
	}
}

static enum command_return
handle_next(mpd_unused struct client *client,
	    mpd_unused int argc, mpd_unused char *argv[])
{
	nextSongInPlaylist();
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_previous(mpd_unused struct client *client,
		mpd_unused int argc, mpd_unused char *argv[])
{
	previousSongInPlaylist();
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_listall(struct client *client, mpd_unused int argc, char *argv[])
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

static enum command_return
handle_volume(struct client *client, mpd_unused int argc, char *argv[])
{
	int change, ret;

	if (check_int(client, &change, argv[1], need_integer) < 0)
		return COMMAND_RETURN_ERROR;

	ret = changeVolumeLevel(change, 1);
	if (ret == -1)
		command_error(client, ACK_ERROR_SYSTEM,
			      "problems setting volume");

	return ret;
}

static enum command_return
handle_setvol(struct client *client, mpd_unused int argc, char *argv[])
{
	int level, ret;

	if (check_int(client, &level, argv[1], need_integer) < 0)
		return COMMAND_RETURN_ERROR;

	ret = changeVolumeLevel(level, 0);
	if (ret == -1)
		command_error(client, ACK_ERROR_SYSTEM,
			      "problems setting volume");

	return ret;
}

static enum command_return
handle_repeat(struct client *client, mpd_unused int argc, char *argv[])
{
	int status;

	if (check_int(client, &status, argv[1], need_integer) < 0)
		return COMMAND_RETURN_ERROR;

	if (status != 0 && status != 1) {
		command_error(client, ACK_ERROR_ARG,
			      "\"%i\" is not 0 or 1", status);
		return COMMAND_RETURN_ERROR;
	}

	setPlaylistRepeatStatus(status);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_random(struct client *client, mpd_unused int argc, char *argv[])
{
	int status;

	if (check_int(client, &status, argv[1], need_integer) < 0)
		return COMMAND_RETURN_ERROR;

	if (status != 0 && status != 1) {
		command_error(client, ACK_ERROR_ARG,
			      "\"%i\" is not 0 or 1", status);
		return COMMAND_RETURN_ERROR;
	}

	setPlaylistRandomStatus(status);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_stats(struct client *client,
	     mpd_unused int argc, mpd_unused char *argv[])
{
	return printStats(client);
}

static enum command_return
handle_clearerror(mpd_unused struct client *client,
		  mpd_unused int argc, mpd_unused char *argv[])
{
	clearPlayerError();
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_list(struct client *client, int argc, char *argv[])
{
	int numConditionals;
	LocateTagItem *conditionals = NULL;
	int tagType = getLocateTagItemType(argv[1]);
	int ret;

	if (tagType < 0) {
		command_error(client, ACK_ERROR_ARG, "\"%s\" is not known", argv[1]);
		return COMMAND_RETURN_ERROR;
	}

	if (tagType == LOCATE_TAG_ANY_TYPE) {
		command_error(client, ACK_ERROR_ARG,
			      "\"any\" is not a valid return tag type");
		return COMMAND_RETURN_ERROR;
	}

	/* for compatibility with < 0.12.0 */
	if (argc == 3) {
		if (tagType != TAG_ITEM_ALBUM) {
			command_error(client, ACK_ERROR_ARG,
				      "should be \"%s\" for 3 arguments",
				      mpdTagItemKeys[TAG_ITEM_ALBUM]);
			return COMMAND_RETURN_ERROR;
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
			return COMMAND_RETURN_ERROR;
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

static enum command_return
handle_move(struct client *client, mpd_unused int argc, char *argv[])
{
	int from, to;
	enum playlist_result result;

	if (check_int(client, &from, argv[1], check_integer, argv[1]) < 0)
		return COMMAND_RETURN_ERROR;
	if (check_int(client, &to, argv[2], check_integer, argv[2]) < 0)
		return COMMAND_RETURN_ERROR;
	result = moveSongInPlaylist(from, to);
	return print_playlist_result(client, result);
}

static enum command_return
handle_moveid(struct client *client, mpd_unused int argc, char *argv[])
{
	int id, to;
	enum playlist_result result;

	if (check_int(client, &id, argv[1], check_integer, argv[1]) < 0)
		return COMMAND_RETURN_ERROR;
	if (check_int(client, &to, argv[2], check_integer, argv[2]) < 0)
		return COMMAND_RETURN_ERROR;
	result = moveSongInPlaylistById(id, to);
	return print_playlist_result(client, result);
}

static enum command_return
handle_swap(struct client *client, mpd_unused int argc, char *argv[])
{
	int song1, song2;
	enum playlist_result result;

	if (check_int(client, &song1, argv[1], check_integer, argv[1]) < 0)
		return COMMAND_RETURN_ERROR;
	if (check_int(client, &song2, argv[2], check_integer, argv[2]) < 0)
		return COMMAND_RETURN_ERROR;
	result = swapSongsInPlaylist(song1, song2);
	return print_playlist_result(client, result);
}

static enum command_return
handle_swapid(struct client *client, mpd_unused int argc, char *argv[])
{
	int id1, id2;
	enum playlist_result result;

	if (check_int(client, &id1, argv[1], check_integer, argv[1]) < 0)
		return COMMAND_RETURN_ERROR;
	if (check_int(client, &id2, argv[2], check_integer, argv[2]) < 0)
		return COMMAND_RETURN_ERROR;
	result = swapSongsInPlaylistById(id1, id2);
	return print_playlist_result(client, result);
}

static enum command_return
handle_seek(struct client *client, mpd_unused int argc, char *argv[])
{
	int song, seek_time;
	enum playlist_result result;

	if (check_int(client, &song, argv[1], check_integer, argv[1]) < 0)
		return COMMAND_RETURN_ERROR;
	if (check_int(client, &seek_time, argv[2], check_integer, argv[2]) < 0)
		return COMMAND_RETURN_ERROR;

	result = seekSongInPlaylist(song, seek_time);
	return print_playlist_result(client, result);
}

static enum command_return
handle_seekid(struct client *client, mpd_unused int argc, char *argv[])
{
	int id, seek_time;
	enum playlist_result result;

	if (check_int(client, &id, argv[1], check_integer, argv[1]) < 0)
		return COMMAND_RETURN_ERROR;
	if (check_int(client, &seek_time, argv[2], check_integer, argv[2]) < 0)
		return COMMAND_RETURN_ERROR;

	result = seekSongInPlaylistById(id, seek_time);
	return print_playlist_result(client, result);
}

static enum command_return
handle_listallinfo(struct client *client, mpd_unused int argc, char *argv[])
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

static enum command_return
handle_ping(mpd_unused struct client *client,
	    mpd_unused int argc, mpd_unused char *argv[])
{
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_password(struct client *client, mpd_unused int argc, char *argv[])
{
	unsigned permission = 0;

	if (getPermissionFromPassword(argv[1], &permission) < 0) {
		command_error(client, ACK_ERROR_PASSWORD, "incorrect password");
		return COMMAND_RETURN_ERROR;
	}

	client_set_permission(client, permission);

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_crossfade(struct client *client, mpd_unused int argc, char *argv[])
{
	int xfade_time;

	if (check_int(client, &xfade_time, argv[1], check_non_negative, argv[1]) < 0)
		return COMMAND_RETURN_ERROR;
	setPlayerCrossFade(xfade_time);

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_enableoutput(struct client *client, mpd_unused int argc, char *argv[])
{
	int device, ret;

	if (check_int(client, &device, argv[1], check_non_negative, argv[1]) < 0)
		return COMMAND_RETURN_ERROR;

	ret = enableAudioDevice(device);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "No such audio output");

	return ret;
}

static enum command_return
handle_disableoutput(struct client *client, mpd_unused int argc, char *argv[])
{
	int device, ret;

	if (check_int(client, &device, argv[1], check_non_negative, argv[1]) < 0)
		return COMMAND_RETURN_ERROR;

	ret = disableAudioDevice(device);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "No such audio output");

	return ret;
}

static enum command_return
handle_devices(struct client *client,
	       mpd_unused int argc, mpd_unused char *argv[])
{
	printAudioDevices(client);

	return COMMAND_RETURN_OK;
}

/* don't be fooled, this is the command handler for "commands" command */
static enum command_return
handle_commands(struct client *client,
		mpd_unused int argc, mpd_unused char *argv[]);

static enum command_return
handle_not_commands(struct client *client,
		    mpd_unused int argc, mpd_unused char *argv[]);

static enum command_return
handle_playlistclear(struct client *client, mpd_unused int argc, char *argv[])
{
	enum playlist_result result;

	result = spl_clear(argv[1]);
	return print_playlist_result(client, result);
}

static enum command_return
handle_playlistadd(struct client *client, mpd_unused int argc, char *argv[])
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
		return COMMAND_RETURN_ERROR;
	}

	return print_playlist_result(client, result);
}

static enum command_return
handle_listplaylists(struct client *client,
		     mpd_unused int argc, mpd_unused char *argv[])
{
	GPtrArray *list = spl_list();
	if (list == NULL) {
		command_error(client, ACK_ERROR_SYSTEM,
			      "failed to get list of stored playlists");
		return COMMAND_RETURN_ERROR;
	}

	print_spl_list(client, list);
	spl_list_free(list);
	return COMMAND_RETURN_OK;
}

static enum command_return
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
	{ "add", PERMISSION_ADD, 1, 1, handle_add },
	{ "addid", PERMISSION_ADD, 1, 2, handle_addid },
	{ "clear", PERMISSION_CONTROL, 0, 0, handle_clear },
	{ "clearerror", PERMISSION_CONTROL, 0, 0, handle_clearerror },
	{ "close", PERMISSION_NONE, -1, -1, handle_close },
	{ "commands", PERMISSION_NONE, 0, 0, handle_commands },
	{ "count", PERMISSION_READ, 2, -1, handle_count },
	{ "crossfade", PERMISSION_CONTROL, 1, 1, handle_crossfade },
	{ "currentsong", PERMISSION_READ, 0, 0, handle_currentsong },
	{ "delete", PERMISSION_CONTROL, 1, 1, handle_delete },
	{ "deleteid", PERMISSION_CONTROL, 1, 1, handle_deleteid },
	{ "disableoutput", PERMISSION_ADMIN, 1, 1, handle_disableoutput },
	{ "enableoutput", PERMISSION_ADMIN, 1, 1, handle_enableoutput },
	{ "find", PERMISSION_READ, 2, -1, handle_find },
	{ "idle", PERMISSION_READ, 0, 0, handle_idle },
	{ "kill", PERMISSION_ADMIN, -1, -1, handle_kill },
	{ "list", PERMISSION_READ, 1, -1, handle_list },
	{ "listall", PERMISSION_READ, 0, 1, handle_listall },
	{ "listallinfo", PERMISSION_READ, 0, 1, handle_listallinfo },
	{ "listplaylist", PERMISSION_READ, 1, 1, handle_listplaylist },
	{ "listplaylistinfo", PERMISSION_READ, 1, 1, handle_listplaylistinfo },
	{ "listplaylists", PERMISSION_READ, 0, 0, handle_listplaylists },
	{ "load", PERMISSION_ADD, 1, 1, handle_load },
	{ "lsinfo", PERMISSION_READ, 0, 1, handle_lsinfo },
	{ "move", PERMISSION_CONTROL, 2, 2, handle_move },
	{ "moveid", PERMISSION_CONTROL, 2, 2, handle_moveid },
	{ "next", PERMISSION_CONTROL, 0, 0, handle_next },
	{ "notcommands", PERMISSION_NONE, 0, 0, handle_not_commands },
	{ "outputs", PERMISSION_READ, 0, 0, handle_devices },
	{ "password", PERMISSION_NONE, 1, 1, handle_password },
	{ "pause", PERMISSION_CONTROL, 0, 1, handle_pause },
	{ "ping", PERMISSION_NONE, 0, 0, handle_ping },
	{ "play", PERMISSION_CONTROL, 0, 1, handle_play },
	{ "playid", PERMISSION_CONTROL, 0, 1, handle_playid },
	{ "playlist", PERMISSION_READ, 0, 0, handle_playlist },
	{ "playlistadd", PERMISSION_CONTROL, 2, 2, handle_playlistadd },
	{ "playlistclear", PERMISSION_CONTROL, 1, 1, handle_playlistclear },
	{ "playlistdelete", PERMISSION_CONTROL, 2, 2, handle_playlistdelete },
	{ "playlistfind", PERMISSION_READ, 2, -1, handle_playlistfind },
	{ "playlistid", PERMISSION_READ, 0, 1, handle_playlistid },
	{ "playlistinfo", PERMISSION_READ, 0, 1, handle_playlistinfo },
	{ "playlistmove", PERMISSION_CONTROL, 3, 3, handle_playlistmove },
	{ "playlistsearch", PERMISSION_READ, 2, -1, handle_playlistsearch },
	{ "plchanges", PERMISSION_READ, 1, 1, handle_plchanges },
	{ "plchangesposid", PERMISSION_READ, 1, 1, handle_plchangesposid },
	{ "previous", PERMISSION_CONTROL, 0, 0, handle_previous },
	{ "random", PERMISSION_CONTROL, 1, 1, handle_random },
	{ "rename", PERMISSION_CONTROL, 2, 2, handle_rename },
	{ "repeat", PERMISSION_CONTROL, 1, 1, handle_repeat },
	{ "rm", PERMISSION_CONTROL, 1, 1, handle_rm },
	{ "save", PERMISSION_CONTROL, 1, 1, handle_save },
	{ "search", PERMISSION_READ, 2, -1, handle_search },
	{ "seek", PERMISSION_CONTROL, 2, 2, handle_seek },
	{ "seekid", PERMISSION_CONTROL, 2, 2, handle_seekid },
	{ "setvol", PERMISSION_CONTROL, 1, 1, handle_setvol },
	{ "shuffle", PERMISSION_CONTROL, 0, 0, handle_shuffle },
	{ "stats", PERMISSION_READ, 0, 0, handle_stats },
	{ "status", PERMISSION_READ, 0, 0, handle_status },
	{ "stop", PERMISSION_CONTROL, 0, 0, handle_stop },
	{ "swap", PERMISSION_CONTROL, 2, 2, handle_swap },
	{ "swapid", PERMISSION_CONTROL, 2, 2, handle_swapid },
	{ "tagtypes", PERMISSION_READ, 0, 0, handle_tagtypes },
	{ "update", PERMISSION_ADMIN, 0, 1, handle_update },
	{ "urlhandlers", PERMISSION_READ, 0, 0, handle_urlhandlers },
	{ "volume", PERMISSION_CONTROL, 1, 1, handle_volume },
};

static const unsigned num_commands = sizeof(commands) / sizeof(commands[0]);

/* don't be fooled, this is the command handler for "commands" command */
static enum command_return
handle_commands(struct client *client,
		mpd_unused int argc, mpd_unused char *argv[])
{
	const unsigned permission = client_get_permission(client);
	const struct command *cmd;

	for (unsigned i = 0; i < num_commands; ++i) {
		cmd = &commands[i];

		if (cmd->permission == (permission & cmd->permission))
			client_printf(client, "command: %s\n", cmd->cmd);
	}

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_not_commands(struct client *client,
		    mpd_unused int argc, mpd_unused char *argv[])
{
	const unsigned permission = client_get_permission(client);
	const struct command *cmd;

	for (unsigned i = 0; i < num_commands; ++i) {
		cmd = &commands[i];

		if (cmd->permission != (permission & cmd->permission))
			client_printf(client, "command: %s\n", cmd->cmd);
	}

	return COMMAND_RETURN_OK;
}

void command_init(void)
{
#ifndef NDEBUG
	/* ensure that the command list is sorted */
	for (unsigned i = 0; i < num_commands - 1; ++i)
		assert(strcmp(commands[i].cmd, commands[i + 1].cmd) < 0);
#endif
}

void command_finish(void)
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
command_check_request(const struct command *cmd, struct client *client,
		      unsigned permission, int argc, char *argv[])
{
	int min = cmd->min + 1;
	int max = cmd->max + 1;

	if (cmd->permission != (permission & cmd->permission)) {
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
command_checked_lookup(struct client *client, unsigned permission,
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

	if (command_check_request(cmd, client, permission, argc, argv) < 0)
		return NULL;

	return cmd;
}

enum command_return
command_process(struct client *client, char *commandString)
{
	int argc;
	char *argv[COMMAND_ARGV_MAX] = { NULL };
	const struct command *cmd;
	enum command_return ret = COMMAND_RETURN_ERROR;

	if (!(argc = buffer2array(commandString, argv, COMMAND_ARGV_MAX)))
		return COMMAND_RETURN_OK;

	cmd = command_checked_lookup(client, client_get_permission(client),
				     argc, argv);
	if (cmd)
		ret = cmd->handler(client, argc, argv);

	current_command = NULL;

	return ret;
}

enum command_return
command_process_list(struct client *client,
		     int list_ok, struct strnode *list)
{
	struct strnode *cur = list;
	enum command_return ret = COMMAND_RETURN_OK;

	command_list_num = 0;

	while (cur) {
		DEBUG("command_process_list: process command \"%s\"\n",
		      cur->data);
		ret = command_process(client, cur->data);
		DEBUG("command_process_list: command returned %i\n", ret);
		if (ret != COMMAND_RETURN_OK || client_is_expired(client))
			break;
		else if (list_ok)
			client_puts(client, "list_OK\n");
		command_list_num++;
		cur = cur->next;
	}

	command_list_num = 0;
	return ret;
}
