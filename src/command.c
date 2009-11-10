/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "command.h"
#include "player_control.h"
#include "playlist.h"
#include "playlist_print.h"
#include "playlist_save.h"
#include "playlist_queue.h"
#include "queue_print.h"
#include "ls.h"
#include "uri.h"
#include "decoder_print.h"
#include "directory.h"
#include "directory_print.h"
#include "database.h"
#include "update.h"
#include "volume.h"
#include "stats.h"
#include "permission.h"
#include "tokenizer.h"
#include "stored_playlist.h"
#include "ack.h"
#include "output_command.h"
#include "output_print.h"
#include "locate.h"
#include "dbUtils.h"
#include "tag.h"
#include "client.h"
#include "tag_print.h"
#include "path.h"
#include "replay_gain.h"
#include "idle.h"

#ifdef ENABLE_SQLITE
#include "sticker.h"
#include "sticker_print.h"
#include "song_sticker.h"
#include "song_print.h"
#endif

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

#define COMMAND_STATUS_STATE            "state"
#define COMMAND_STATUS_REPEAT           "repeat"
#define COMMAND_STATUS_SINGLE           "single"
#define COMMAND_STATUS_CONSUME          "consume"
#define COMMAND_STATUS_RANDOM           "random"
#define COMMAND_STATUS_PLAYLIST         "playlist"
#define COMMAND_STATUS_PLAYLIST_LENGTH  "playlistlength"
#define COMMAND_STATUS_SONG             "song"
#define COMMAND_STATUS_SONGID           "songid"
#define COMMAND_STATUS_NEXTSONG         "nextsong"
#define COMMAND_STATUS_NEXTSONGID       "nextsongid"
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
static const char need_range[] = "need a range";

/* FIXME: redundant error messages */
static const char check_integer[] = "\"%s\" is not a integer";
static const char need_integer[] = "need an integer";

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

G_GNUC_PRINTF(3, 4) static void command_error(struct client *client, enum ack error,
				       const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	command_error_v(client, error, fmt, args);
	va_end(args);
}

static bool G_GNUC_PRINTF(4, 5)
check_uint32(struct client *client, uint32_t *dst,
	     const char *s, const char *fmt, ...)
{
	char *test;

	*dst = strtoul(s, &test, 10);
	if (*test != '\0') {
		va_list args;
		va_start(args, fmt);
		command_error_v(client, ACK_ERROR_ARG, fmt, args);
		va_end(args);
		return false;
	}
	return true;
}

static bool G_GNUC_PRINTF(4, 5)
check_int(struct client *client, int *value_r,
	  const char *s, const char *fmt, ...)
{
	char *test;
	long value;

	value = strtol(s, &test, 10);
	if (*test != '\0') {
		va_list args;
		va_start(args, fmt);
		command_error_v(client, ACK_ERROR_ARG, fmt, args);
		va_end(args);
		return false;
	}

#if G_MAXLONG > G_MAXINT
	if (value < G_MININT || value > G_MAXINT) {
		command_error(client, ACK_ERROR_ARG,
			      "Number too large: %s", s);
		return false;
	}
#endif

	*value_r = (int)value;
	return true;
}

static bool G_GNUC_PRINTF(5, 6)
check_range(struct client *client, unsigned *value_r1, unsigned *value_r2,
	    const char *s, const char *fmt, ...)
{
	char *test, *test2;
	long value;

	value = strtol(s, &test, 10);
	if (*test != '\0' && *test != ':') {
		va_list args;
		va_start(args, fmt);
		command_error_v(client, ACK_ERROR_ARG, fmt, args);
		va_end(args);
		return false;
	}

	if (value == -1 && *test == 0) {
		/* compatibility with older MPD versions: specifying
		   "-1" makes MPD display the whole list */
		*value_r1 = 0;
		*value_r2 = G_MAXUINT;
		return true;
	}

	if (value < 0) {
		command_error(client, ACK_ERROR_ARG,
			      "Number is negative: %s", s);
		return false;
	}

#if G_MAXLONG > G_MAXUINT
	if (value > G_MAXUINT) {
		command_error(client, ACK_ERROR_ARG,
			      "Number too large: %s", s);
		return false;
	}
#endif

	*value_r1 = (unsigned)value;

	if (*test == ':') {
		value = strtol(++test, &test2, 10);
		if (*test2 != '\0') {
			va_list args;
			va_start(args, fmt);
			command_error_v(client, ACK_ERROR_ARG, fmt, args);
			va_end(args);
			return false;
		}

		if (test == test2)
			value = G_MAXUINT;

		if (value < 0) {
			command_error(client, ACK_ERROR_ARG,
				      "Number is negative: %s", s);
			return false;
		}

#if G_MAXLONG > G_MAXUINT
		if (value > G_MAXUINT) {
			command_error(client, ACK_ERROR_ARG,
				      "Number too large: %s", s);
			return false;
		}
#endif
		*value_r2 = (unsigned)value;
	} else {
		*value_r2 = (unsigned)value + 1;
	}

	return true;
}

static bool
check_unsigned(struct client *client, unsigned *value_r, const char *s)
{
	unsigned long value;
	char *endptr;

	value = strtoul(s, &endptr, 10);
	if (*endptr != 0) {
		command_error(client, ACK_ERROR_ARG,
			      "Integer expected: %s", s);
		return false;
	}

	if (value > G_MAXUINT) {
		command_error(client, ACK_ERROR_ARG,
			      "Number too large: %s", s);
		return false;
	}

	*value_r = (unsigned)value;
	return true;
}

static bool
check_bool(struct client *client, bool *value_r, const char *s)
{
	long value;
	char *endptr;

	value = strtol(s, &endptr, 10);
	if (*endptr != 0 || (value != 0 && value != 1)) {
		command_error(client, ACK_ERROR_ARG,
			      "Boolean (0/1) expected: %s", s);
		return false;
	}

	*value_r = !!value;
	return true;
}

static enum command_return
print_playlist_result(struct client *client,
		      enum playlist_result result)
{
	switch (result) {
	case PLAYLIST_RESULT_SUCCESS:
		return COMMAND_RETURN_OK;

	case PLAYLIST_RESULT_ERRNO:
		command_error(client, ACK_ERROR_SYSTEM, "%s", strerror(errno));
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

	case PLAYLIST_RESULT_DISABLED:
		command_error(client, ACK_ERROR_UNKNOWN,
			      "stored playlist support is disabled");
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
#ifndef WIN32
		struct tm tm;
#endif
		char timestamp[32];

		client_printf(client, "playlist: %s\n", playlist->name);

		t = playlist->mtime;
		strftime(timestamp, sizeof(timestamp), "%FT%TZ",
#ifdef WIN32
			 gmtime(&t)
#else
			 gmtime_r(&t, &tm)
#endif
			 );
		client_printf(client, "Last-Modified: %s\n", timestamp);
	}
}

static enum command_return
handle_urlhandlers(struct client *client,
		   G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	if (client_get_uid(client) > 0)
		client_puts(client, "handler: file://\n");
	print_supported_uri_schemes(client);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_decoders(struct client *client,
		G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	decoder_list_print(client);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_tagtypes(struct client *client,
		G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	tag_print_types(client);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_play(struct client *client, int argc, char *argv[])
{
	int song = -1;
	enum playlist_result result;

	if (argc == 2 && !check_int(client, &song, argv[1], need_positive))
		return COMMAND_RETURN_ERROR;
	result = playlist_play(&g_playlist, song);
	return print_playlist_result(client, result);
}

static enum command_return
handle_playid(struct client *client, int argc, char *argv[])
{
	int id = -1;
	enum playlist_result result;

	if (argc == 2 && !check_int(client, &id, argv[1], need_positive))
		return COMMAND_RETURN_ERROR;

	result = playlist_play_id(&g_playlist, id);
	return print_playlist_result(client, result);
}

static enum command_return
handle_stop(G_GNUC_UNUSED struct client *client,
	    G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	playlist_stop(&g_playlist);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_currentsong(struct client *client,
		   G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	playlist_print_current(client, &g_playlist);
	return PLAYLIST_RESULT_SUCCESS;
}

static enum command_return
handle_pause(struct client *client,
	     int argc, char *argv[])
{
	if (argc == 2) {
		bool pause_flag;
		if (!check_bool(client, &pause_flag, argv[1]))
			return COMMAND_RETURN_ERROR;

		pc_set_pause(pause_flag);
	} else
		pc_pause();

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_status(struct client *client,
	      G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	const char *state = NULL;
	struct player_status player_status;
	int updateJobId;
	char *error;
	int song;

	pc_get_status(&player_status);

	switch (player_status.state) {
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
		      "volume: %i\n"
		      COMMAND_STATUS_REPEAT ": %i\n"
		      COMMAND_STATUS_RANDOM ": %i\n"
		      COMMAND_STATUS_SINGLE ": %i\n"
		      COMMAND_STATUS_CONSUME ": %i\n"
		      COMMAND_STATUS_PLAYLIST ": %li\n"
		      COMMAND_STATUS_PLAYLIST_LENGTH ": %i\n"
		      COMMAND_STATUS_CROSSFADE ": %i\n"
		      COMMAND_STATUS_STATE ": %s\n",
		      volume_level_get(),
		      playlist_get_repeat(&g_playlist),
		      playlist_get_random(&g_playlist),
		      playlist_get_single(&g_playlist),
		      playlist_get_consume(&g_playlist),
		      playlist_get_version(&g_playlist),
		      playlist_get_length(&g_playlist),
		      (int)(pc_get_cross_fade() + 0.5),
		      state);

	song = playlist_get_current_song(&g_playlist);
	if (song >= 0) {
		client_printf(client,
			      COMMAND_STATUS_SONG ": %i\n"
			      COMMAND_STATUS_SONGID ": %u\n",
			      song, playlist_get_song_id(&g_playlist, song));
	}

	if (player_status.state != PLAYER_STATE_STOP) {
		struct audio_format_string af_string;

		client_printf(client,
			      COMMAND_STATUS_TIME ": %i:%i\n"
			      "elapsed: %1.3f\n"
			      COMMAND_STATUS_BITRATE ": %u\n"
			      COMMAND_STATUS_AUDIO ": %s\n",
			      (int)(player_status.elapsed_time + 0.5),
			      (int)(player_status.total_time + 0.5),
			      player_status.elapsed_time,
			      player_status.bit_rate,
			      audio_format_to_string(&player_status.audio_format,
						     &af_string));
	}

	if ((updateJobId = isUpdatingDB())) {
		client_printf(client,
			      COMMAND_STATUS_UPDATING_DB ": %i\n",
			      updateJobId);
	}

	error = pc_get_error_message();
	if (error != NULL) {
		client_printf(client,
			      COMMAND_STATUS_ERROR ": %s\n",
			      error);
		g_free(error);
	}

	song = playlist_get_next_song(&g_playlist);
	if (song >= 0) {
		client_printf(client,
			      COMMAND_STATUS_NEXTSONG ": %i\n"
			      COMMAND_STATUS_NEXTSONGID ": %u\n",
			      song, playlist_get_song_id(&g_playlist, song));
	}

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_kill(G_GNUC_UNUSED struct client *client,
	    G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	return COMMAND_RETURN_KILL;
}

static enum command_return
handle_close(G_GNUC_UNUSED struct client *client,
	     G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	return COMMAND_RETURN_CLOSE;
}

static enum command_return
handle_add(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	char *uri = argv[1];
	enum playlist_result result;

	if (strncmp(uri, "file:///", 8) == 0) {
#ifdef WIN32
		result = PLAYLIST_RESULT_DENIED;
#else
		result = playlist_append_file(&g_playlist,
					      uri + 7, client_get_uid(client),
					      NULL);
#endif
		return print_playlist_result(client, result);
	}

	if (uri_has_scheme(uri)) {
		if (!uri_supported_scheme(uri)) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported URI scheme");
			return COMMAND_RETURN_ERROR;
		}

		result = playlist_append_uri(&g_playlist, uri, NULL);
		return print_playlist_result(client, result);
	}

	result = addAllIn(uri);
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
	char *uri = argv[1];
	unsigned added_id;
	enum playlist_result result;

	if (strncmp(uri, "file:///", 8) == 0) {
#ifdef WIN32
		result = PLAYLIST_RESULT_DENIED;
#else
		result = playlist_append_file(&g_playlist, uri + 7,
					      client_get_uid(client),
					      &added_id);
#endif
	} else {
		if (uri_has_scheme(uri) && !uri_supported_scheme(uri)) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported URI scheme");
			return COMMAND_RETURN_ERROR;
		}

		result = playlist_append_uri(&g_playlist, uri, &added_id);
	}

	if (result != PLAYLIST_RESULT_SUCCESS)
		return print_playlist_result(client, result);

	if (argc == 3) {
		int to;
		if (!check_int(client, &to, argv[2], check_integer, argv[2]))
			return COMMAND_RETURN_ERROR;
		result = playlist_move_id(&g_playlist, added_id, to);
		if (result != PLAYLIST_RESULT_SUCCESS) {
			enum command_return ret =
				print_playlist_result(client, result);
			playlist_delete_id(&g_playlist, added_id);
			return ret;
		}
	}

	client_printf(client, "Id: %u\n", added_id);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_delete(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned start, end;
	enum playlist_result result;

	if (!check_range(client, &start, &end, argv[1], need_range))
		return COMMAND_RETURN_ERROR;

	result = playlist_delete_range(&g_playlist, start, end);
	return print_playlist_result(client, result);
}

static enum command_return
handle_deleteid(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	int id;
	enum playlist_result result;

	if (!check_int(client, &id, argv[1], need_positive))
		return COMMAND_RETURN_ERROR;

	result = playlist_delete_id(&g_playlist, id);
	return print_playlist_result(client, result);
}

static enum command_return
handle_playlist(struct client *client,
	        G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	playlist_print_uris(client, &g_playlist);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_shuffle(G_GNUC_UNUSED struct client *client,
	       G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	unsigned start = 0, end = queue_length(&g_playlist.queue);
	if (argc == 2 && !check_range(client, &start, &end,
	                              argv[1], need_range))
		return COMMAND_RETURN_ERROR;

	playlist_shuffle(&g_playlist, start, end);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_clear(G_GNUC_UNUSED struct client *client,
	     G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	playlist_clear(&g_playlist);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_save(struct client *client,
	    G_GNUC_UNUSED int argc, char *argv[])
{
	enum playlist_result result;

	result = spl_save_playlist(argv[1], &g_playlist);
	return print_playlist_result(client, result);
}

static enum command_return
handle_load(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	enum playlist_result result;

	result = playlist_open_into_queue(argv[1], &g_playlist);
	if (result != PLAYLIST_RESULT_NO_SUCH_LIST)
		return result;

	result = playlist_load_spl(&g_playlist, argv[1]);
	return print_playlist_result(client, result);
}

static enum command_return
handle_listplaylist(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	bool ret;

	ret = spl_print(client, argv[1], false);
	if (!ret) {
		command_error(client, ACK_ERROR_NO_EXIST, "No such playlist");
		return COMMAND_RETURN_ERROR;
	}

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_listplaylistinfo(struct client *client,
			G_GNUC_UNUSED int argc, char *argv[])
{
	bool ret;

	ret = spl_print(client, argv[1], true);
	if (!ret) {
		command_error(client, ACK_ERROR_NO_EXIST, "No such playlist");
		return COMMAND_RETURN_ERROR;
	}

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_lsinfo(struct client *client, int argc, char *argv[])
{
	const char *uri;
	const struct directory *directory;

	if (argc == 2)
		uri = argv[1];
	else
		/* default is root directory */
		uri = "";

	directory = db_get_directory(uri);
	if (directory == NULL) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory not found");
		return COMMAND_RETURN_ERROR;
	}

	directory_print(client, directory);

	if (isRootDirectory(uri)) {
		GPtrArray *list = spl_list();
		if (list != NULL) {
			print_spl_list(client, list);
			spl_list_free(list);
		}
	}

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_rm(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	enum playlist_result result;

	result = spl_delete(argv[1]);
	return print_playlist_result(client, result);
}

static enum command_return
handle_rename(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	enum playlist_result result;

	result = spl_rename(argv[1], argv[2]);
	return print_playlist_result(client, result);
}

static enum command_return
handle_plchanges(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	uint32_t version;

	if (!check_uint32(client, &version, argv[1], need_positive))
		return COMMAND_RETURN_ERROR;

	playlist_print_changes_info(client, &g_playlist, version);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_plchangesposid(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	uint32_t version;

	if (!check_uint32(client, &version, argv[1], need_positive))
		return COMMAND_RETURN_ERROR;

	playlist_print_changes_position(client, &g_playlist, version);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_playlistinfo(struct client *client, int argc, char *argv[])
{
	unsigned start = 0, end = G_MAXUINT;
	bool ret;

	if (argc == 2 && !check_range(client, &start, &end,
				      argv[1], need_range))
		return COMMAND_RETURN_ERROR;

	ret = playlist_print_info(client, &g_playlist, start, end);
	if (!ret)
		return print_playlist_result(client,
					     PLAYLIST_RESULT_BAD_RANGE);

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_playlistid(struct client *client, int argc, char *argv[])
{
	int id = -1;

	if (argc == 2 && !check_int(client, &id, argv[1], need_positive))
		return COMMAND_RETURN_ERROR;

	if (id >= 0) {
		bool ret = playlist_print_id(client, &g_playlist, id);
		if (!ret)
			return print_playlist_result(client,
						     PLAYLIST_RESULT_NO_SUCH_SONG);
	} else {
		playlist_print_info(client, &g_playlist, 0, G_MAXUINT);
	}

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_find(struct client *client, int argc, char *argv[])
{
	int ret;
	struct locate_item_list *list =
		locate_item_list_parse(argv + 1, argc - 1);

	if (list == NULL || list->length == 0) {
		if (list != NULL)
			locate_item_list_free(list);

		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	ret = findSongsIn(client, NULL, list);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");

	locate_item_list_free(list);

	return ret;
}

static enum command_return
handle_findadd(struct client *client, int argc, char *argv[])
{
    int ret;
    struct locate_item_list *list =
	    locate_item_list_parse(argv + 1, argc - 1);
    if (list == NULL || list->length == 0) {
	    if (list != NULL)
		    locate_item_list_free(list);

	    command_error(client, ACK_ERROR_ARG, "incorrect arguments");
	    return COMMAND_RETURN_ERROR;
    }

    ret = findAddIn(client, NULL, list);
    if (ret == -1)
	    command_error(client, ACK_ERROR_NO_EXIST,
			  "directory or file not found");

    locate_item_list_free(list);

    return ret;
}

static enum command_return
handle_search(struct client *client, int argc, char *argv[])
{
	int ret;
	struct locate_item_list *list =
		locate_item_list_parse(argv + 1, argc - 1);

	if (list == NULL || list->length == 0) {
		if (list != NULL)
			locate_item_list_free(list);

		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	ret = searchForSongsIn(client, NULL, list);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");

	locate_item_list_free(list);

	return ret;
}

static enum command_return
handle_count(struct client *client, int argc, char *argv[])
{
	int ret;
	struct locate_item_list *list =
		locate_item_list_parse(argv + 1, argc - 1);

	if (list == NULL || list->length == 0) {
		if (list != NULL)
			locate_item_list_free(list);

		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	ret = searchStatsForSongsIn(client, NULL, list);
	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");

	locate_item_list_free(list);

	return ret;
}

static enum command_return
handle_playlistfind(struct client *client, int argc, char *argv[])
{
	struct locate_item_list *list =
		locate_item_list_parse(argv + 1, argc - 1);

	if (list == NULL || list->length == 0) {
		if (list != NULL)
			locate_item_list_free(list);

		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	playlist_print_find(client, &g_playlist, list);

	locate_item_list_free(list);

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_playlistsearch(struct client *client, int argc, char *argv[])
{
	struct locate_item_list *list =
		locate_item_list_parse(argv + 1, argc - 1);

	if (list == NULL || list->length == 0) {
		if (list != NULL)
			locate_item_list_free(list);

		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return COMMAND_RETURN_ERROR;
	}

	playlist_print_search(client, &g_playlist, list);

	locate_item_list_free(list);

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_playlistdelete(struct client *client,
		      G_GNUC_UNUSED int argc, char *argv[]) {
	char *playlist = argv[1];
	int from;
	enum playlist_result result;

	if (!check_int(client, &from, argv[2], check_integer, argv[2]))
		return COMMAND_RETURN_ERROR;

	result = spl_remove_index(playlist, from);
	return print_playlist_result(client, result);
}

static enum command_return
handle_playlistmove(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	char *playlist = argv[1];
	int from, to;
	enum playlist_result result;

	if (!check_int(client, &from, argv[2], check_integer, argv[2]))
		return COMMAND_RETURN_ERROR;
	if (!check_int(client, &to, argv[3], check_integer, argv[3]))
		return COMMAND_RETURN_ERROR;

	result = spl_move_index(playlist, from, to);
	return print_playlist_result(client, result);
}

static enum command_return
handle_update(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	const char *path = NULL;
	unsigned ret;

	assert(argc <= 2);
	if (argc == 2)
		path = argv[1];

	ret = update_enqueue(path, false);
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
handle_rescan(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	const char *path = NULL;
	unsigned ret;

	assert(argc <= 2);
	if (argc == 2)
		path = argv[1];

	ret = update_enqueue(path, true);
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
handle_next(G_GNUC_UNUSED struct client *client,
	    G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	/* single mode is not considered when this is user who
	 * wants to change song. */
	int single = g_playlist.queue.single;
	g_playlist.queue.single = false;

	playlist_next(&g_playlist);

	g_playlist.queue.single = single;
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_previous(G_GNUC_UNUSED struct client *client,
		G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	playlist_previous(&g_playlist);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_listall(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
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
handle_setvol(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	int level;
	bool success;

	if (!check_int(client, &level, argv[1], need_integer))
		return COMMAND_RETURN_ERROR;

	if (level < 0 || level > 100) {
		command_error(client, ACK_ERROR_ARG, "Invalid volume value");
		return COMMAND_RETURN_ERROR;
	}

	success = volume_level_change(level);
	if (!success) {
		command_error(client, ACK_ERROR_SYSTEM,
			      "problems setting volume");
		return COMMAND_RETURN_ERROR;
	}

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_repeat(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	int status;

	if (!check_int(client, &status, argv[1], need_integer))
		return COMMAND_RETURN_ERROR;

	if (status != 0 && status != 1) {
		command_error(client, ACK_ERROR_ARG,
			      "\"%i\" is not 0 or 1", status);
		return COMMAND_RETURN_ERROR;
	}

	playlist_set_repeat(&g_playlist, status);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_single(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	int status;

	if (!check_int(client, &status, argv[1], need_integer))
		return COMMAND_RETURN_ERROR;

	if (status != 0 && status != 1) {
		command_error(client, ACK_ERROR_ARG,
			      "\"%i\" is not 0 or 1", status);
		return COMMAND_RETURN_ERROR;
	}

	playlist_set_single(&g_playlist, status);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_consume(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	int status;

	if (!check_int(client, &status, argv[1], need_integer))
		return COMMAND_RETURN_ERROR;

	if (status != 0 && status != 1) {
		command_error(client, ACK_ERROR_ARG,
			      "\"%i\" is not 0 or 1", status);
		return COMMAND_RETURN_ERROR;
	}

	playlist_set_consume(&g_playlist, status);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_random(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	int status;

	if (!check_int(client, &status, argv[1], need_integer))
		return COMMAND_RETURN_ERROR;

	if (status != 0 && status != 1) {
		command_error(client, ACK_ERROR_ARG,
			      "\"%i\" is not 0 or 1", status);
		return COMMAND_RETURN_ERROR;
	}

	playlist_set_random(&g_playlist, status);
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_stats(struct client *client,
	     G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	return stats_print(client);
}

static enum command_return
handle_clearerror(G_GNUC_UNUSED struct client *client,
		  G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	pc_clear_error();
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_list(struct client *client, int argc, char *argv[])
{
	struct locate_item_list *conditionals;
	int tagType = locate_parse_type(argv[1]);
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
		if (tagType != TAG_ALBUM) {
			command_error(client, ACK_ERROR_ARG,
				      "should be \"%s\" for 3 arguments",
				      tag_item_names[TAG_ALBUM]);
			return COMMAND_RETURN_ERROR;
		}

		locate_item_list_parse(argv + 1, argc - 1);

		conditionals = locate_item_list_new(1);
		conditionals->items[0].tag = TAG_ARTIST;
		conditionals->items[0].needle = g_strdup(argv[2]);
	} else {
		conditionals =
			locate_item_list_parse(argv + 2, argc - 2);
		if (conditionals == NULL) {
			command_error(client, ACK_ERROR_ARG,
				      "not able to parse args");
			return COMMAND_RETURN_ERROR;
		}
	}

	ret = listAllUniqueTags(client, tagType, conditionals);

	locate_item_list_free(conditionals);

	if (ret == -1)
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");

	return ret;
}

static enum command_return
handle_move(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned start, end;
	int to;
	enum playlist_result result;

	if (!check_range(client, &start, &end,
				      argv[1], need_range))
		return COMMAND_RETURN_ERROR;
	if (!check_int(client, &to, argv[2], check_integer, argv[2]))
		return COMMAND_RETURN_ERROR;
	result = playlist_move_range(&g_playlist, start, end, to);
	return print_playlist_result(client, result);
}

static enum command_return
handle_moveid(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	int id, to;
	enum playlist_result result;

	if (!check_int(client, &id, argv[1], check_integer, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_int(client, &to, argv[2], check_integer, argv[2]))
		return COMMAND_RETURN_ERROR;
	result = playlist_move_id(&g_playlist, id, to);
	return print_playlist_result(client, result);
}

static enum command_return
handle_swap(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	int song1, song2;
	enum playlist_result result;

	if (!check_int(client, &song1, argv[1], check_integer, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_int(client, &song2, argv[2], check_integer, argv[2]))
		return COMMAND_RETURN_ERROR;
	result = playlist_swap_songs(&g_playlist, song1, song2);
	return print_playlist_result(client, result);
}

static enum command_return
handle_swapid(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	int id1, id2;
	enum playlist_result result;

	if (!check_int(client, &id1, argv[1], check_integer, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_int(client, &id2, argv[2], check_integer, argv[2]))
		return COMMAND_RETURN_ERROR;
	result = playlist_swap_songs_id(&g_playlist, id1, id2);
	return print_playlist_result(client, result);
}

static enum command_return
handle_seek(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	int song, seek_time;
	enum playlist_result result;

	if (!check_int(client, &song, argv[1], check_integer, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_int(client, &seek_time, argv[2], check_integer, argv[2]))
		return COMMAND_RETURN_ERROR;

	result = playlist_seek_song(&g_playlist, song, seek_time);
	return print_playlist_result(client, result);
}

static enum command_return
handle_seekid(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	int id, seek_time;
	enum playlist_result result;

	if (!check_int(client, &id, argv[1], check_integer, argv[1]))
		return COMMAND_RETURN_ERROR;
	if (!check_int(client, &seek_time, argv[2], check_integer, argv[2]))
		return COMMAND_RETURN_ERROR;

	result = playlist_seek_song_id(&g_playlist, id, seek_time);
	return print_playlist_result(client, result);
}

static enum command_return
handle_listallinfo(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
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
handle_ping(G_GNUC_UNUSED struct client *client,
	    G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_password(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
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
handle_crossfade(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned xfade_time;

	if (!check_unsigned(client, &xfade_time, argv[1]))
		return COMMAND_RETURN_ERROR;
	pc_set_cross_fade(xfade_time);

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_enableoutput(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned device;
	bool ret;

	if (!check_unsigned(client, &device, argv[1]))
		return COMMAND_RETURN_ERROR;

	ret = audio_output_enable_index(device);
	if (!ret) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "No such audio output");
		return COMMAND_RETURN_ERROR;
	}

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_disableoutput(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	unsigned device;
	bool ret;

	if (!check_unsigned(client, &device, argv[1]))
		return COMMAND_RETURN_ERROR;

	ret = audio_output_disable_index(device);
	if (!ret) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "No such audio output");
		return COMMAND_RETURN_ERROR;
	}

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_devices(struct client *client,
	       G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	printAudioDevices(client);

	return COMMAND_RETURN_OK;
}

/* don't be fooled, this is the command handler for "commands" command */
static enum command_return
handle_commands(struct client *client,
		G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[]);

static enum command_return
handle_not_commands(struct client *client,
		    G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[]);

static enum command_return
handle_playlistclear(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	enum playlist_result result;

	result = spl_clear(argv[1]);
	return print_playlist_result(client, result);
}

static enum command_return
handle_playlistadd(struct client *client, G_GNUC_UNUSED int argc, char *argv[])
{
	char *playlist = argv[1];
	char *uri = argv[2];
	enum playlist_result result;

	if (uri_has_scheme(uri)) {
		if (!uri_supported_scheme(uri)) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported URI scheme");
			return COMMAND_RETURN_ERROR;
		}

		result = spl_append_uri(uri, playlist);
	} else
		result = addAllInToStoredPlaylist(uri, playlist);

	if (result == (enum playlist_result)-1) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "directory or file not found");
		return COMMAND_RETURN_ERROR;
	}

	return print_playlist_result(client, result);
}

static enum command_return
handle_listplaylists(struct client *client,
		     G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
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
handle_replay_gain_mode(struct client *client,
			G_GNUC_UNUSED int argc, char *argv[])
{
	if (!replay_gain_set_mode_string(argv[1])) {
		command_error(client, ACK_ERROR_ARG,
			      "Unrecognized replay gain mode");
		return COMMAND_RETURN_ERROR;
	}

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_replay_gain_status(struct client *client,
			  G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	client_printf(client, "replay_gain_mode: %s\n",
		      replay_gain_get_mode_string());
	return COMMAND_RETURN_OK;
}

static enum command_return
handle_idle(struct client *client,
	    G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
        unsigned flags = 0, j;
        int i;
        const char *const* idle_names;

        idle_names = idle_get_names();
        for (i = 1; i < argc; ++i) {
                if (!argv[i])
                        continue;

                for (j = 0; idle_names[j]; ++j) {
                        if (!g_ascii_strcasecmp(argv[i], idle_names[j])) {
                                flags |= (1 << j);
                        }
                }
        }

        /* No argument means that the client wants to receive everything */
        if (flags == 0)
                flags = ~0;

	/* enable "idle" mode on this client */
	client_idle_wait(client, flags);

	/* return value is "1" so the caller won't print "OK" */
	return 1;
}

#ifdef ENABLE_SQLITE
struct sticker_song_find_data {
	struct client *client;
	const char *name;
};

static void
sticker_song_find_print_cb(struct song *song, const char *value,
			   gpointer user_data)
{
	struct sticker_song_find_data *data = user_data;

	song_print_uri(data->client, song);
	sticker_print_value(data->client, data->name, value);
}

static enum command_return
handle_sticker_song(struct client *client, int argc, char *argv[])
{
	/* get song song_id key */
	if (argc == 5 && strcmp(argv[1], "get") == 0) {
		struct song *song;
		char *value;

		song = db_get_song(argv[3]);
		if (song == NULL) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "no such song");
			return COMMAND_RETURN_ERROR;
		}

		value = sticker_song_get_value(song, argv[4]);
		if (value == NULL) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "no such sticker");
			return COMMAND_RETURN_ERROR;
		}

		sticker_print_value(client, argv[4], value);
		g_free(value);

		return COMMAND_RETURN_OK;
	/* list song song_id */
	} else if (argc == 4 && strcmp(argv[1], "list") == 0) {
		struct song *song;
		struct sticker *sticker;

		song = db_get_song(argv[3]);
		if (song == NULL) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "no such song");
			return COMMAND_RETURN_ERROR;
		}

		sticker = sticker_song_get(song);
		if (NULL == sticker) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "no stickers found");
			return COMMAND_RETURN_ERROR;
		}

		sticker_print(client, sticker);
		sticker_free(sticker);

		return COMMAND_RETURN_OK;
	/* set song song_id id key */
	} else if (argc == 6 && strcmp(argv[1], "set") == 0) {
		struct song *song;
		bool ret;

		song = db_get_song(argv[3]);
		if (song == NULL) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "no such song");
			return COMMAND_RETURN_ERROR;
		}

		ret = sticker_song_set_value(song, argv[4], argv[5]);
		if (!ret) {
			command_error(client, ACK_ERROR_SYSTEM,
				      "failed to set sticker value");
			return COMMAND_RETURN_ERROR;
		}

		return COMMAND_RETURN_OK;
	/* delete song song_id [key] */
	} else if ((argc == 4 || argc == 5) &&
		   strcmp(argv[1], "delete") == 0) {
		struct song *song;
		bool ret;

		song = db_get_song(argv[3]);
		if (song == NULL) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "no such song");
			return COMMAND_RETURN_ERROR;
		}

		ret = argc == 4
			? sticker_song_delete(song)
			: sticker_song_delete_value(song, argv[4]);
		if (!ret) {
			command_error(client, ACK_ERROR_SYSTEM,
				      "no such sticker");
			return COMMAND_RETURN_ERROR;
		}

		return COMMAND_RETURN_OK;
	/* find song dir key */
	} else if (argc == 5 && strcmp(argv[1], "find") == 0) {
		/* "sticker find song a/directory name" */
		struct directory *directory;
		bool success;
		struct sticker_song_find_data data = {
			.client = client,
			.name = argv[4],
		};

		directory = db_get_directory(argv[3]);
		if (directory == NULL) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "no such directory");
			return COMMAND_RETURN_ERROR;
		}

		success = sticker_song_find(directory, data.name,
					    sticker_song_find_print_cb, &data);
		if (!success) {
			command_error(client, ACK_ERROR_SYSTEM,
				      "failed to set search sticker database");
			return COMMAND_RETURN_ERROR;
		}

		return COMMAND_RETURN_OK;
	} else {
		command_error(client, ACK_ERROR_ARG, "bad request");
		return COMMAND_RETURN_ERROR;
	}
}

static enum command_return
handle_sticker(struct client *client, int argc, char *argv[])
{
	assert(argc >= 4);

	if (!sticker_enabled()) {
		command_error(client, ACK_ERROR_UNKNOWN,
			      "sticker database is disabled");
		return COMMAND_RETURN_ERROR;
	}

	if (strcmp(argv[2], "song") == 0)
		return handle_sticker_song(client, argc, argv);
	else {
		command_error(client, ACK_ERROR_ARG,
			      "unknown sticker domain");
		return COMMAND_RETURN_ERROR;
	}
}
#endif

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
	{ "consume", PERMISSION_CONTROL, 1, 1, handle_consume },
	{ "count", PERMISSION_READ, 2, -1, handle_count },
	{ "crossfade", PERMISSION_CONTROL, 1, 1, handle_crossfade },
	{ "currentsong", PERMISSION_READ, 0, 0, handle_currentsong },
	{ "decoders", PERMISSION_READ, 0, 0, handle_decoders },
	{ "delete", PERMISSION_CONTROL, 1, 1, handle_delete },
	{ "deleteid", PERMISSION_CONTROL, 1, 1, handle_deleteid },
	{ "disableoutput", PERMISSION_ADMIN, 1, 1, handle_disableoutput },
	{ "enableoutput", PERMISSION_ADMIN, 1, 1, handle_enableoutput },
	{ "find", PERMISSION_READ, 2, -1, handle_find },
	{ "findadd", PERMISSION_READ, 2, -1, handle_findadd},
	{ "idle", PERMISSION_READ, 0, -1, handle_idle },
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
	{ "replay_gain_mode", PERMISSION_CONTROL, 1, 1,
	  handle_replay_gain_mode },
	{ "replay_gain_status", PERMISSION_READ, 0, 0,
	  handle_replay_gain_status },
	{ "rescan", PERMISSION_ADMIN, 0, 1, handle_rescan },
	{ "rm", PERMISSION_CONTROL, 1, 1, handle_rm },
	{ "save", PERMISSION_CONTROL, 1, 1, handle_save },
	{ "search", PERMISSION_READ, 2, -1, handle_search },
	{ "seek", PERMISSION_CONTROL, 2, 2, handle_seek },
	{ "seekid", PERMISSION_CONTROL, 2, 2, handle_seekid },
	{ "setvol", PERMISSION_CONTROL, 1, 1, handle_setvol },
	{ "shuffle", PERMISSION_CONTROL, 0, 1, handle_shuffle },
	{ "single", PERMISSION_CONTROL, 1, 1, handle_single },
	{ "stats", PERMISSION_READ, 0, 0, handle_stats },
	{ "status", PERMISSION_READ, 0, 0, handle_status },
#ifdef ENABLE_SQLITE
	{ "sticker", PERMISSION_ADMIN, 3, -1, handle_sticker },
#endif
	{ "stop", PERMISSION_CONTROL, 0, 0, handle_stop },
	{ "swap", PERMISSION_CONTROL, 2, 2, handle_swap },
	{ "swapid", PERMISSION_CONTROL, 2, 2, handle_swapid },
	{ "tagtypes", PERMISSION_READ, 0, 0, handle_tagtypes },
	{ "update", PERMISSION_ADMIN, 0, 1, handle_update },
	{ "urlhandlers", PERMISSION_READ, 0, 0, handle_urlhandlers },
};

static const unsigned num_commands = sizeof(commands) / sizeof(commands[0]);

static bool
command_available(G_GNUC_UNUSED const struct command *cmd)
{
#ifdef ENABLE_SQLITE
	if (strcmp(cmd->cmd, "sticker") == 0)
		return sticker_enabled();
#endif

	return true;
}

/* don't be fooled, this is the command handler for "commands" command */
static enum command_return
handle_commands(struct client *client,
		G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
{
	const unsigned permission = client_get_permission(client);
	const struct command *cmd;

	for (unsigned i = 0; i < num_commands; ++i) {
		cmd = &commands[i];

		if (cmd->permission == (permission & cmd->permission) &&
		    command_available(cmd))
			client_printf(client, "command: %s\n", cmd->cmd);
	}

	return COMMAND_RETURN_OK;
}

static enum command_return
handle_not_commands(struct client *client,
		    G_GNUC_UNUSED int argc, G_GNUC_UNUSED char *argv[])
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

static bool
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
		return false;
	}

	if (min == 0)
		return true;

	if (min == max && max != argc) {
		if (client != NULL)
			command_error(client, ACK_ERROR_ARG,
				      "wrong number of arguments for \"%s\"",
				      argv[0]);
		return false;
	} else if (argc < min) {
		if (client != NULL)
			command_error(client, ACK_ERROR_ARG,
				      "too few arguments for \"%s\"", argv[0]);
		return false;
	} else if (argc > max && max /* != 0 */ ) {
		if (client != NULL)
			command_error(client, ACK_ERROR_ARG,
				      "too many arguments for \"%s\"", argv[0]);
		return false;
	} else
		return true;
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

	if (!command_check_request(cmd, client, permission, argc, argv))
		return NULL;

	return cmd;
}

enum command_return
command_process(struct client *client, unsigned num, char *line)
{
	GError *error = NULL;
	int argc;
	char *argv[COMMAND_ARGV_MAX] = { NULL };
	const struct command *cmd;
	enum command_return ret = COMMAND_RETURN_ERROR;

	command_list_num = num;

	/* get the command name (first word on the line) */

	argv[0] = tokenizer_next_word(&line, &error);
	if (argv[0] == NULL) {
		current_command = "";
		if (*line == 0)
			command_error(client, ACK_ERROR_UNKNOWN,
				      "No command given");
		else {
			command_error(client, ACK_ERROR_UNKNOWN,
				      "%s", error->message);
			g_error_free(error);
		}
		current_command = NULL;

		return COMMAND_RETURN_ERROR;
	}

	argc = 1;

	/* now parse the arguments (quoted or unquoted) */

	while (argc < (int)G_N_ELEMENTS(argv) &&
	       (argv[argc] =
		tokenizer_next_param(&line, &error)) != NULL)
		++argc;

	/* some error checks; we have to set current_command because
	   command_error() expects it to be set */

	current_command = argv[0];

	if (argc >= (int)G_N_ELEMENTS(argv)) {
		command_error(client, ACK_ERROR_ARG, "Too many arguments");
		current_command = NULL;
		return COMMAND_RETURN_ERROR;
	}

	if (*line != 0) {
		command_error(client, ACK_ERROR_ARG,
			      "%s", error->message);
		current_command = NULL;
		g_error_free(error);
		return COMMAND_RETURN_ERROR;
	}

	/* look up and invoke the command handler */

	cmd = command_checked_lookup(client, client_get_permission(client),
				     argc, argv);
	if (cmd)
		ret = cmd->handler(client, argc, argv);

	current_command = NULL;
	command_list_num = 0;

	return ret;
}
