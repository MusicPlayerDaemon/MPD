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
#include "player.h"
#include "playlist.h"
#include "ls.h"
#include "directory.h"
#include "volume.h"
#include "stats.h"
#include "myfprintf.h"
#include "list.h"
#include "permission.h"
#include "buffer2array.h"
#include "log.h"
#include "tag.h"
#include "utils.h"
#include "storedPlaylist.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define COMMAND_PLAY           	"play"
#define COMMAND_PLAYID         	"playid"
#define COMMAND_STOP           	"stop"
#define COMMAND_PAUSE          	"pause"
#define COMMAND_STATUS         	"status"
#define COMMAND_KILL           	"kill"
#define COMMAND_CLOSE          	"close"
#define COMMAND_ADD            	"add"
#define COMMAND_ADDID		"addid"
#define COMMAND_DELETE         	"delete"
#define COMMAND_DELETEID       	"deleteid"
#define COMMAND_PLAYLIST       	"playlist"
#define COMMAND_SHUFFLE        	"shuffle"
#define COMMAND_CLEAR          	"clear"
#define COMMAND_SAVE           	"save"
#define COMMAND_LOAD           	"load"
#define COMMAND_LISTPLAYLIST   	"listplaylist"
#define COMMAND_LISTPLAYLISTINFO   	"listplaylistinfo"
#define COMMAND_LSINFO         	"lsinfo"
#define COMMAND_RM             	"rm"
#define COMMAND_PLAYLISTINFO   	"playlistinfo"
#define COMMAND_PLAYLISTID   	"playlistid"
#define COMMAND_FIND           	"find"
#define COMMAND_SEARCH         	"search"
#define COMMAND_UPDATE         	"update"
#define COMMAND_NEXT           	"next"
#define COMMAND_PREVIOUS       	"previous"
#define COMMAND_LISTALL        	"listall"
#define COMMAND_VOLUME         	"volume"
#define COMMAND_REPEAT         	"repeat"
#define COMMAND_RANDOM         	"random"
#define COMMAND_STATS          	"stats"
#define COMMAND_CLEAR_ERROR    	"clearerror"
#define COMMAND_LIST           	"list"
#define COMMAND_MOVE           	"move"
#define COMMAND_MOVEID         	"moveid"
#define COMMAND_SWAP           	"swap"
#define COMMAND_SWAPID      	"swapid"
#define COMMAND_SEEK           	"seek"
#define COMMAND_SEEKID         	"seekid"
#define COMMAND_LISTALLINFO	"listallinfo"
#define COMMAND_PING		"ping"
#define COMMAND_SETVOL		"setvol"
#define COMMAND_PASSWORD	"password"
#define COMMAND_CROSSFADE	"crossfade"
#define COMMAND_URL_HANDLERS   	"urlhandlers"
#define COMMAND_PLCHANGES	"plchanges"
#define COMMAND_PLCHANGESPOSID	"plchangesposid"
#define COMMAND_CURRENTSONG	"currentsong"
#define COMMAND_ENABLE_DEV	"enableoutput"
#define COMMAND_DISABLE_DEV	"disableoutput"
#define COMMAND_DEVICES		"outputs"
#define COMMAND_COMMANDS	"commands"
#define COMMAND_NOTCOMMANDS	"notcommands"
#define COMMAND_PLAYLISTCLEAR   "playlistclear"
#define COMMAND_PLAYLISTADD	"playlistadd"
#define COMMAND_PLAYLISTFIND	"playlistfind"
#define COMMAND_PLAYLISTSEARCH	"playlistsearch"
#define COMMAND_PLAYLISTMOVE	"playlistmove"
#define COMMAND_PLAYLISTDELETE	"playlistdelete"
#define COMMAND_TAGTYPES	"tagtypes"
#define COMMAND_COUNT		"count"
#define COMMAND_RENAME		"rename"

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

typedef struct _CommandEntry CommandEntry;

typedef int (*CommandHandlerFunction) (int, int *, int, char **);
typedef int (*CommandListHandlerFunction)
 (int, int *, int, char **, struct strnode *, CommandEntry *);

/* if min: -1 don't check args *
 * if max: -1 no max args      */
struct _CommandEntry {
	char *cmd;
	int min;
	int max;
	int reqPermission;
	CommandHandlerFunction handler;
	CommandListHandlerFunction listHandler;
};

static char *current_command;
static int command_listNum;

static CommandEntry *getCommandEntryFromString(char *string, int *permission);

static List *commandList;

static CommandEntry *newCommandEntry(void)
{
	CommandEntry *cmd = xmalloc(sizeof(CommandEntry));
	cmd->cmd = NULL;
	cmd->min = 0;
	cmd->max = 0;
	cmd->handler = NULL;
	cmd->listHandler = NULL;
	cmd->reqPermission = 0;
	return cmd;
}

static void addCommand(char *name,
		       int reqPermission,
		       int minargs,
		       int maxargs,
		       CommandHandlerFunction handler_func,
		       CommandListHandlerFunction listHandler_func)
{
	CommandEntry *cmd = newCommandEntry();
	cmd->cmd = name;
	cmd->min = minargs;
	cmd->max = maxargs;
	cmd->handler = handler_func;
	cmd->listHandler = listHandler_func;
	cmd->reqPermission = reqPermission;

	insertInList(commandList, cmd->cmd, cmd);
}

static int handleUrlHandlers(int fd, int *permission, int argc, char *argv[])
{
	return printRemoteUrlHandlers(fd);
}

static int handleTagTypes(int fd, int *permission, int argc, char *argv[])
{
	printTagTypes(fd);
	return 0;
}

static int handlePlay(int fd, int *permission, int argc, char *argv[])
{
	int song = -1;
	char *test;

	if (argc == 2) {
		song = strtol(argv[1], &test, 10);
		if (*test != '\0') {
			commandError(fd, ACK_ERROR_ARG,
				     "need a positive integer");
			return -1;
		}
	}
	return playPlaylist(fd, song, 0);
}

static int handlePlayId(int fd, int *permission, int argc, char *argv[])
{
	int id = -1;
	char *test;

	if (argc == 2) {
		id = strtol(argv[1], &test, 10);
		if (*test != '\0') {
			commandError(fd, ACK_ERROR_ARG,
				     "need a positive integer");
			return -1;
		}
	}
	return playPlaylistById(fd, id, 0);
}

static int handleStop(int fd, int *permission, int argc, char *argv[])
{
	return stopPlaylist(fd);
}

static int handleCurrentSong(int fd, int *permission, int argc, char *argv[])
{
	int song = getPlaylistCurrentSong();

	if (song >= 0) {
		return playlistInfo(fd, song);
	} else
		return 0;
}

static int handlePause(int fd, int *permission, int argc, char *argv[])
{
	if (argc == 2) {
		char *test;
		int pause = strtol(argv[1], &test, 10);
		if (*test != '\0' || (pause != 0 && pause != 1)) {
			commandError(fd, ACK_ERROR_ARG, "\"%s\" is not 0 or 1",
				     argv[1]);
			return -1;
		}
		return playerSetPause(fd, pause);
	}
	return playerPause(fd);
}

static int commandStatus(int fd, int *permission, int argc, char *argv[])
{
	char *state = NULL;
	int updateJobId;
	int song;

	/*syncPlayerAndPlaylist(); */
	playPlaylistIfPlayerStopped();
	switch (getPlayerState()) {
	case PLAYER_STATE_STOP:
		state = COMMAND_STOP;
		break;
	case PLAYER_STATE_PAUSE:
		state = COMMAND_PAUSE;
		break;
	case PLAYER_STATE_PLAY:
		state = COMMAND_PLAY;
		break;
	}

	fdprintf(fd, "%s: %i\n", COMMAND_STATUS_VOLUME, getVolumeLevel());
	fdprintf(fd, "%s: %i\n", COMMAND_STATUS_REPEAT,
		 getPlaylistRepeatStatus());
	fdprintf(fd, "%s: %i\n", COMMAND_STATUS_RANDOM,
		 getPlaylistRandomStatus());
	fdprintf(fd, "%s: %li\n", COMMAND_STATUS_PLAYLIST,
		 getPlaylistVersion());
	fdprintf(fd, "%s: %i\n", COMMAND_STATUS_PLAYLIST_LENGTH,
		 getPlaylistLength());
	fdprintf(fd, "%s: %i\n", COMMAND_STATUS_CROSSFADE,
		 (int)(getPlayerCrossFade() + 0.5));

	fdprintf(fd, "%s: %s\n", COMMAND_STATUS_STATE, state);

	song = getPlaylistCurrentSong();
	if (song >= 0) {
		fdprintf(fd, "%s: %i\n", COMMAND_STATUS_SONG, song);
		fdprintf(fd, "%s: %i\n", COMMAND_STATUS_SONGID,
			 getPlaylistSongId(song));
	}
	if (getPlayerState() != PLAYER_STATE_STOP) {
		fdprintf(fd, "%s: %i:%i\n", COMMAND_STATUS_TIME,
			 getPlayerElapsedTime(), getPlayerTotalTime());
		fdprintf(fd, "%s: %li\n", COMMAND_STATUS_BITRATE,
			 getPlayerBitRate());
		fdprintf(fd, "%s: %u:%i:%i\n", COMMAND_STATUS_AUDIO,
			 getPlayerSampleRate(), getPlayerBits(),
			 getPlayerChannels());
	}

	if ((updateJobId = isUpdatingDB())) {
		fdprintf(fd, "%s: %i\n", COMMAND_STATUS_UPDATING_DB,
			 updateJobId);
	}

	if (getPlayerError() != PLAYER_ERROR_NOERROR) {
		fdprintf(fd, "%s: %s\n", COMMAND_STATUS_ERROR,
			 getPlayerErrorStr());
	}

	return 0;
}

static int handleKill(int fd, int *permission, int argc, char *argv[])
{
	return COMMAND_RETURN_KILL;
}

static int handleClose(int fd, int *permission, int argc, char *argv[])
{
	return COMMAND_RETURN_CLOSE;
}

static int handleAdd(int fd, int *permission, int argc, char *argv[])
{
	char *path = argv[1];

	if (isRemoteUrl(path))
		return addToPlaylist(fd, path, 0);

	return addAllIn(fd, path);
}

static int handleAddId(int fd, int *permission, int argc, char *argv[])
{
	return addToPlaylist(fd, argv[1], 1);
}

static int handleDelete(int fd, int *permission, int argc, char *argv[])
{
	int song;
	char *test;

	song = strtol(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG, "need a positive integer");
		return -1;
	}
	return deleteFromPlaylist(fd, song);
}

static int handleDeleteId(int fd, int *permission, int argc, char *argv[])
{
	int id;
	char *test;

	id = strtol(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG, "need a positive integer");
		return -1;
	}
	return deleteFromPlaylistById(fd, id);
}

static int handlePlaylist(int fd, int *permission, int argc, char *argv[])
{
	return showPlaylist(fd);
}

static int handleShuffle(int fd, int *permission, int argc, char *argv[])
{
	return shufflePlaylist(fd);
}

static int handleClear(int fd, int *permission, int argc, char *argv[])
{
	return clearPlaylist(fd);
}

static int handleSave(int fd, int *permission, int argc, char *argv[])
{
	return savePlaylist(fd, argv[1]);
}

static int handleLoad(int fd, int *permission, int argc, char *argv[])
{
	return loadPlaylist(fd, argv[1]);
}

static int handleListPlaylist(int fd, int *permission, int argc, char *argv[])
{
	return PlaylistInfo(fd, argv[1], 0);
}

static int handleListPlaylistInfo(int fd, int *permission,
				  int argc, char *argv[])
{
	return PlaylistInfo(fd, argv[1], 1);
}

static int handleLsInfo(int fd, int *permission, int argc, char *argv[])
{
	char *path = "";

	if (argc == 2)
		path = argv[1];

	if (printDirectoryInfo(fd, path) < 0)
		return -1;

	if (isRootDirectory(path))
		return lsPlaylists(fd, path);

	return 0;
}

static int handleRm(int fd, int *permission, int argc, char *argv[])
{
	return deletePlaylist(fd, argv[1]);
}

static int handleRename(int fd, int *permission, int argc, char *argv[])
{
	return renameStoredPlaylist(fd, argv[1], argv[2]);
}

static int handlePlaylistChanges(int fd, int *permission,
				 int argc, char *argv[])
{
	unsigned long version;
	char *test;

	version = strtoul(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG, "need a positive integer");
		return -1;
	}
	return playlistChanges(fd, version);
}

static int handlePlaylistChangesPosId(int fd, int *permission,
				      int argc, char *argv[])
{
	unsigned long version;
	char *test;

	version = strtoul(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG, "need a positive integer");
		return -1;
	}
	return playlistChangesPosId(fd, version);
}

static int handlePlaylistInfo(int fd, int *permission, int argc, char *argv[])
{
	int song = -1;
	char *test;

	if (argc == 2) {
		song = strtol(argv[1], &test, 10);
		if (*test != '\0') {
			commandError(fd, ACK_ERROR_ARG,
				     "need a positive integer");
			return -1;
		}
	}
	return playlistInfo(fd, song);
}

static int handlePlaylistId(int fd, int *permission, int argc, char *argv[])
{
	int id = -1;
	char *test;

	if (argc == 2) {
		id = strtol(argv[1], &test, 10);
		if (*test != '\0') {
			commandError(fd, ACK_ERROR_ARG,
				     "need a positive integer");
			return -1;
		}
	}
	return playlistId(fd, id);
}

static int handleFind(int fd, int *permission, int argc, char *argv[])
{
	int ret;

	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		commandError(fd, ACK_ERROR_ARG, "incorrect arguments");
		return -1;
	}

	ret = findSongsIn(fd, NULL, numItems, items);

	freeLocateTagItemArray(numItems, items);

	return ret;
}

static int handleSearch(int fd, int *permission, int argc, char *argv[])
{
	int ret;

	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		commandError(fd, ACK_ERROR_ARG, "incorrect arguments");
		return -1;
	}

	ret = searchForSongsIn(fd, NULL, numItems, items);

	freeLocateTagItemArray(numItems, items);

	return ret;
}

static int handleCount(int fd, int *permission, int argc, char *argv[])
{
	int ret;

	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		commandError(fd, ACK_ERROR_ARG, "incorrect arguments");
		return -1;
	}

	ret = searchStatsForSongsIn(fd, NULL, numItems, items);

	freeLocateTagItemArray(numItems, items);

	return ret;
}

static int handlePlaylistFind(int fd, int *permission, int argc, char *argv[])
{
	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		commandError(fd, ACK_ERROR_ARG, "incorrect arguments");
		return -1;
	}

	findSongsInPlaylist(fd, numItems, items);

	freeLocateTagItemArray(numItems, items);

	return 0;
}

static int handlePlaylistSearch(int fd, int *permission, int argc, char *argv[])
{
	LocateTagItem *items;
	int numItems = newLocateTagItemArrayFromArgArray(argv + 1,
							 argc - 1,
							 &items);

	if (numItems <= 0) {
		commandError(fd, ACK_ERROR_ARG, "incorrect arguments");
		return -1;
	}

	searchForSongsInPlaylist(fd, numItems, items);

	freeLocateTagItemArray(numItems, items);

	return 0;
}

static int handlePlaylistDelete(int fd, int *permission, int argc, char *argv[]) {
	char *playlist = argv[1];
	int from;
	char *test;

	from = strtol(argv[2], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer", argv[2]);
		return -1;
	}

	return removeOneSongFromStoredPlaylistByPath(fd, playlist, from);
}

static int handlePlaylistMove(int fd, int *permission, int argc, char *argv[])
{
	char *playlist = argv[1];
	int from, to;
	char *test;

	from = strtol(argv[2], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer", argv[2]);
		return -1;
	}
	to = strtol(argv[3], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer", argv[3]);
		return -1;
	}

	return moveSongInStoredPlaylistByPath(fd, playlist, from, to);
}

static int listHandleUpdate(int fd,
			    int *permission,
			    int argc,
			    char *argv[],
			    struct strnode *cmdnode, CommandEntry * cmd)
{
	static List *pathList;
	CommandEntry *nextCmd = NULL;
	struct strnode *next = cmdnode->next;

	if (!pathList)
		pathList = makeList(NULL, 1);

	if (argc == 2)
		insertInList(pathList, argv[1], NULL);
	else
		insertInList(pathList, "", NULL);

	if (next)
		nextCmd = getCommandEntryFromString(next->data, permission);

	if (cmd != nextCmd) {
		int ret = updateInit(fd, pathList);
		freeList(pathList);
		pathList = NULL;
		return ret;
	}

	return 0;
}

static int handleUpdate(int fd, int *permission, int argc, char *argv[])
{
	if (argc == 2) {
		int ret;
		List *pathList = makeList(NULL, 1);
		insertInList(pathList, argv[1], NULL);
		ret = updateInit(fd, pathList);
		freeList(pathList);
		return ret;
	}
	return updateInit(fd, NULL);
}

static int handleNext(int fd, int *permission, int argc, char *argv[])
{
	return nextSongInPlaylist(fd);
}

static int handlePrevious(int fd, int *permission, int argc, char *argv[])
{
	return previousSongInPlaylist(fd);
}

static int handleListAll(int fd, int *permission, int argc, char *argv[])
{
	char *directory = NULL;

	if (argc == 2)
		directory = argv[1];
	return printAllIn(fd, directory);
}

static int handleVolume(int fd, int *permission, int argc, char *argv[])
{
	int change;
	char *test;

	change = strtol(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG, "need an integer");
		return -1;
	}
	return changeVolumeLevel(fd, change, 1);
}

static int handleSetVol(int fd, int *permission, int argc, char *argv[])
{
	int level;
	char *test;

	level = strtol(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG, "need an integer");
		return -1;
	}
	return changeVolumeLevel(fd, level, 0);
}

static int handleRepeat(int fd, int *permission, int argc, char *argv[])
{
	int status;
	char *test;

	status = strtol(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG, "need an integer");
		return -1;
	}
	return setPlaylistRepeatStatus(fd, status);
}

static int handleRandom(int fd, int *permission, int argc, char *argv[])
{
	int status;
	char *test;

	status = strtol(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG, "need an integer");
		return -1;
	}
	return setPlaylistRandomStatus(fd, status);
}

static int handleStats(int fd, int *permission, int argc, char *argv[])
{
	return printStats(fd);
}

static int handleClearError(int fd, int *permission, int argc, char *argv[])
{
	clearPlayerError();
	return 0;
}

static int handleList(int fd, int *permission, int argc, char *argv[])
{
	int numConditionals = 0;
	LocateTagItem *conditionals = NULL;
	int tagType = getLocateTagItemType(argv[1]);
	int ret;

	if (tagType < 0) {
		commandError(fd, ACK_ERROR_ARG, "\"%s\" is not known", argv[1]);
		return -1;
	}

	if (tagType == LOCATE_TAG_ANY_TYPE) {
		commandError(fd, ACK_ERROR_ARG,
		             "\"any\" is not a valid return tag type");
		return -1;
	}

	/* for compatibility with < 0.12.0 */
	if (argc == 3) {
		if (tagType != TAG_ITEM_ALBUM) {
			commandError(fd, ACK_ERROR_ARG,
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
			commandError(fd, ACK_ERROR_ARG,
				     "not able to parse args");
			return -1;
		}
	}

	ret = listAllUniqueTags(fd, tagType, numConditionals, conditionals);

	if (conditionals)
		freeLocateTagItemArray(numConditionals, conditionals);

	return ret;
}

static int handleMove(int fd, int *permission, int argc, char *argv[])
{
	int from;
	int to;
	char *test;

	from = strtol(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer", argv[1]);
		return -1;
	}
	to = strtol(argv[2], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer", argv[2]);
		return -1;
	}
	return moveSongInPlaylist(fd, from, to);
}

static int handleMoveId(int fd, int *permission, int argc, char *argv[])
{
	int id;
	int to;
	char *test;

	id = strtol(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer", argv[1]);
		return -1;
	}
	to = strtol(argv[2], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer", argv[2]);
		return -1;
	}
	return moveSongInPlaylistById(fd, id, to);
}

static int handleSwap(int fd, int *permission, int argc, char *argv[])
{
	int song1;
	int song2;
	char *test;

	song1 = strtol(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer", argv[1]);
		return -1;
	}
	song2 = strtol(argv[2], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG, "\"%s\" is not a integer",
			     argv[2]);
		return -1;
	}
	return swapSongsInPlaylist(fd, song1, song2);
}

static int handleSwapId(int fd, int *permission, int argc, char *argv[])
{
	int id1;
	int id2;
	char *test;

	id1 = strtol(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer", argv[1]);
		return -1;
	}
	id2 = strtol(argv[2], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG, "\"%s\" is not a integer",
			     argv[2]);
		return -1;
	}
	return swapSongsInPlaylistById(fd, id1, id2);
}

static int handleSeek(int fd, int *permission, int argc, char *argv[])
{
	int song;
	int time;
	char *test;

	song = strtol(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer", argv[1]);
		return -1;
	}
	time = strtol(argv[2], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer", argv[2]);
		return -1;
	}
	return seekSongInPlaylist(fd, song, time);
}

static int handleSeekId(int fd, int *permission, int argc, char *argv[])
{
	int id;
	int time;
	char *test;

	id = strtol(argv[1], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer", argv[1]);
		return -1;
	}
	time = strtol(argv[2], &test, 10);
	if (*test != '\0') {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer", argv[2]);
		return -1;
	}
	return seekSongInPlaylistById(fd, id, time);
}

static int handleListAllInfo(int fd, int *permission, int argc, char *argv[])
{
	char *directory = NULL;

	if (argc == 2)
		directory = argv[1];
	return printInfoForAllIn(fd, directory);
}

static int handlePing(int fd, int *permission, int argc, char *argv[])
{
	return 0;
}

static int handlePassword(int fd, int *permission, int argc, char *argv[])
{
	if (getPermissionFromPassword(argv[1], permission) < 0) {
		commandError(fd, ACK_ERROR_PASSWORD, "incorrect password");
		return -1;
	}

	return 0;
}

static int handleCrossfade(int fd, int *permission, int argc, char *argv[])
{
	int time;
	char *test;

	time = strtol(argv[1], &test, 10);
	if (*test != '\0' || time < 0) {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer >= 0", argv[1]);
		return -1;
	}

	setPlayerCrossFade(time);

	return 0;
}

static int handleEnableDevice(int fd, int *permission, int argc, char *argv[])
{
	int device;
	char *test;

	device = strtol(argv[1], &test, 10);
	if (*test != '\0' || device < 0) {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer >= 0", argv[1]);
		return -1;
	}

	return enableAudioDevice(fd, device);
}

static int handleDisableDevice(int fd, int *permission, int argc, char *argv[])
{
	int device;
	char *test;

	device = strtol(argv[1], &test, 10);
	if (*test != '\0' || device < 0) {
		commandError(fd, ACK_ERROR_ARG,
			     "\"%s\" is not a integer >= 0", argv[1]);
		return -1;
	}

	return disableAudioDevice(fd, device);
}

static int handleDevices(int fd, int *permission, int argc, char *argv[])
{
	printAudioDevices(fd);

	return 0;
}

/* don't be fooled, this is the command handler for "commands" command */
static int handleCommands(int fd, int *permission, int argc, char *argv[])
{
	ListNode *node = commandList->firstNode;
	CommandEntry *cmd;

	while (node != NULL) {
		cmd = (CommandEntry *) node->data;
		if (cmd->reqPermission == (*permission & cmd->reqPermission)) {
			fdprintf(fd, "command: %s\n", cmd->cmd);
		}

		node = node->nextNode;
	}

	return 0;
}

static int handleNotcommands(int fd, int *permission, int argc, char *argv[])
{
	ListNode *node = commandList->firstNode;
	CommandEntry *cmd;

	while (node != NULL) {
		cmd = (CommandEntry *) node->data;

		if (cmd->reqPermission != (*permission & cmd->reqPermission)) {
			fdprintf(fd, "command: %s\n", cmd->cmd);
		}

		node = node->nextNode;
	}

	return 0;
}

static int handlePlaylistClear(int fd, int *permission, int argc, char *argv[])
{
	return clearStoredPlaylist(fd, argv[1]);
}

static int handlePlaylistAdd(int fd, int *permission, int argc, char *argv[])
{
	char *playlist = argv[1];
	char *path = argv[2];

	if (isRemoteUrl(path))
		return addToStoredPlaylist(fd, path, playlist);

	return addAllInToStoredPlaylist(fd, path, playlist);
}

void initCommands(void)
{
	commandList = makeList(free, 1);

	/* addCommand(name,                  permission,         min, max, handler,                    list handler); */
	addCommand(COMMAND_PLAY,             PERMISSION_CONTROL, 0,   1,   handlePlay,                 NULL);
	addCommand(COMMAND_PLAYID,           PERMISSION_CONTROL, 0,   1,   handlePlayId,               NULL);
	addCommand(COMMAND_STOP,             PERMISSION_CONTROL, 0,   0,   handleStop,                 NULL);
	addCommand(COMMAND_CURRENTSONG,      PERMISSION_READ,    0,   0,   handleCurrentSong,          NULL);
	addCommand(COMMAND_PAUSE,            PERMISSION_CONTROL, 0,   1,   handlePause,                NULL);
	addCommand(COMMAND_STATUS,           PERMISSION_READ,    0,   0,   commandStatus,              NULL);
	addCommand(COMMAND_KILL,             PERMISSION_ADMIN,   -1,  -1,  handleKill,                 NULL);
	addCommand(COMMAND_CLOSE,            PERMISSION_NONE,    -1,  -1,  handleClose,                NULL);
	addCommand(COMMAND_ADD,              PERMISSION_ADD,     1,   1,   handleAdd,                  NULL);
	addCommand(COMMAND_ADDID,            PERMISSION_ADD,     1,   1,   handleAddId,                NULL);
	addCommand(COMMAND_DELETE,           PERMISSION_CONTROL, 1,   1,   handleDelete,               NULL);
	addCommand(COMMAND_DELETEID,         PERMISSION_CONTROL, 1,   1,   handleDeleteId,             NULL);
	addCommand(COMMAND_PLAYLIST,         PERMISSION_READ,    0,   0,   handlePlaylist,             NULL);
	addCommand(COMMAND_PLAYLISTID,       PERMISSION_READ,    0,   1,   handlePlaylistId,           NULL);
	addCommand(COMMAND_SHUFFLE,          PERMISSION_CONTROL, 0,   0,   handleShuffle,              NULL);
	addCommand(COMMAND_CLEAR,            PERMISSION_CONTROL, 0,   0,   handleClear,                NULL);
	addCommand(COMMAND_SAVE,             PERMISSION_CONTROL, 1,   1,   handleSave,                 NULL);
	addCommand(COMMAND_LOAD,             PERMISSION_ADD,     1,   1,   handleLoad,                 NULL);
	addCommand(COMMAND_LISTPLAYLIST,     PERMISSION_READ,    1,   1,   handleListPlaylist,         NULL);
	addCommand(COMMAND_LISTPLAYLISTINFO, PERMISSION_READ,    1,   1,   handleListPlaylistInfo,     NULL);
	addCommand(COMMAND_LSINFO,           PERMISSION_READ,    0,   1,   handleLsInfo,               NULL);
	addCommand(COMMAND_RM,               PERMISSION_CONTROL, 1,   1,   handleRm,                   NULL);
	addCommand(COMMAND_PLAYLISTINFO,     PERMISSION_READ,    0,   1,   handlePlaylistInfo,         NULL);
	addCommand(COMMAND_FIND,             PERMISSION_READ,    2,   -1,  handleFind,                 NULL);
	addCommand(COMMAND_SEARCH,           PERMISSION_READ,    2,   -1,  handleSearch,               NULL);
	addCommand(COMMAND_UPDATE,           PERMISSION_ADMIN,   0,   1,   handleUpdate,               listHandleUpdate);
	addCommand(COMMAND_NEXT,             PERMISSION_CONTROL, 0,   0,   handleNext,                 NULL);
	addCommand(COMMAND_PREVIOUS,         PERMISSION_CONTROL, 0,   0,   handlePrevious,             NULL);
	addCommand(COMMAND_LISTALL,          PERMISSION_READ,    0,   1,   handleListAll,              NULL);
	addCommand(COMMAND_VOLUME,           PERMISSION_CONTROL, 1,   1,   handleVolume,               NULL);
	addCommand(COMMAND_REPEAT,           PERMISSION_CONTROL, 1,   1,   handleRepeat,               NULL);
	addCommand(COMMAND_RANDOM,           PERMISSION_CONTROL, 1,   1,   handleRandom,               NULL);
	addCommand(COMMAND_STATS,            PERMISSION_READ,    0,   0,   handleStats,                NULL);
	addCommand(COMMAND_CLEAR_ERROR,      PERMISSION_CONTROL, 0,   0,   handleClearError,           NULL);
	addCommand(COMMAND_LIST,             PERMISSION_READ,    1,   -1,  handleList,                 NULL);
	addCommand(COMMAND_MOVE,             PERMISSION_CONTROL, 2,   2,   handleMove,                 NULL);
	addCommand(COMMAND_MOVEID,           PERMISSION_CONTROL, 2,   2,   handleMoveId,               NULL);
	addCommand(COMMAND_SWAP,             PERMISSION_CONTROL, 2,   2,   handleSwap,                 NULL);
	addCommand(COMMAND_SWAPID,           PERMISSION_CONTROL, 2,   2,   handleSwapId,               NULL);
	addCommand(COMMAND_SEEK,             PERMISSION_CONTROL, 2,   2,   handleSeek,                 NULL);
	addCommand(COMMAND_SEEKID,           PERMISSION_CONTROL, 2,   2,   handleSeekId,               NULL);
	addCommand(COMMAND_LISTALLINFO,      PERMISSION_READ,    0,   1,   handleListAllInfo,          NULL);
	addCommand(COMMAND_PING,             PERMISSION_NONE,    0,   0,   handlePing,                 NULL);
	addCommand(COMMAND_SETVOL,           PERMISSION_CONTROL, 1,   1,   handleSetVol,               NULL);
	addCommand(COMMAND_PASSWORD,         PERMISSION_NONE,    1,   1,   handlePassword,             NULL);
	addCommand(COMMAND_CROSSFADE,        PERMISSION_CONTROL, 1,   1,   handleCrossfade,            NULL);
	addCommand(COMMAND_URL_HANDLERS,     PERMISSION_READ,    0,   0,   handleUrlHandlers,          NULL);
	addCommand(COMMAND_PLCHANGES,        PERMISSION_READ,    1,   1,   handlePlaylistChanges,      NULL);
	addCommand(COMMAND_PLCHANGESPOSID,   PERMISSION_READ,    1,   1,   handlePlaylistChangesPosId, NULL);
	addCommand(COMMAND_ENABLE_DEV,       PERMISSION_ADMIN,   1,   1,   handleEnableDevice,         NULL);
	addCommand(COMMAND_DISABLE_DEV,      PERMISSION_ADMIN,   1,   1,   handleDisableDevice,        NULL);
	addCommand(COMMAND_DEVICES,          PERMISSION_READ,    0,   0,   handleDevices,              NULL);
	addCommand(COMMAND_COMMANDS,         PERMISSION_NONE,    0,   0,   handleCommands,             NULL);
	addCommand(COMMAND_NOTCOMMANDS,      PERMISSION_NONE,    0,   0,   handleNotcommands,          NULL);
	addCommand(COMMAND_PLAYLISTCLEAR,    PERMISSION_CONTROL, 1,   1,   handlePlaylistClear,        NULL);
	addCommand(COMMAND_PLAYLISTADD,      PERMISSION_CONTROL, 2,   2,   handlePlaylistAdd,          NULL);
	addCommand(COMMAND_PLAYLISTFIND,     PERMISSION_READ,    2,   -1,  handlePlaylistFind,         NULL);
	addCommand(COMMAND_PLAYLISTSEARCH,   PERMISSION_READ,    2,   -1,  handlePlaylistSearch,       NULL);
	addCommand(COMMAND_PLAYLISTMOVE,     PERMISSION_CONTROL, 3,   3,   handlePlaylistMove,         NULL);
	addCommand(COMMAND_PLAYLISTDELETE,   PERMISSION_CONTROL, 2,   2,   handlePlaylistDelete,       NULL);
	addCommand(COMMAND_TAGTYPES,         PERMISSION_READ,    0,   0,   handleTagTypes,             NULL);
	addCommand(COMMAND_COUNT,            PERMISSION_READ,    2,   -1,  handleCount,                NULL);
	addCommand(COMMAND_RENAME,           PERMISSION_CONTROL, 2,   2,   handleRename,               NULL);

	sortList(commandList);
}

void finishCommands(void)
{
	freeList(commandList);
}

static int checkArgcAndPermission(CommandEntry * cmd, int fd,
				  int permission, int argc, char *argv[])
{
	int min = cmd->min + 1;
	int max = cmd->max + 1;

	if (cmd->reqPermission != (permission & cmd->reqPermission)) {
		if (fd) {
			commandError(fd, ACK_ERROR_PERMISSION,
				     "you don't have permission for \"%s\"",
				     cmd->cmd);
		}
		return -1;
	}

	if (min == 0)
		return 0;

	if (min == max && max != argc) {
		if (fd) {
			commandError(fd, ACK_ERROR_ARG,
				     "wrong number of arguments for \"%s\"",
				     argv[0]);
		}
		return -1;
	} else if (argc < min) {
		if (fd) {
			commandError(fd, ACK_ERROR_ARG,
				     "too few arguments for \"%s\"", argv[0]);
		}
		return -1;
	} else if (argc > max && max /* != 0 */ ) {
		if (fd) {
			commandError(fd, ACK_ERROR_ARG,
				     "too many arguments for \"%s\"", argv[0]);
		}
		return -1;
	} else
		return 0;
}

static CommandEntry *getCommandEntryAndCheckArgcAndPermission(int fd,
							      int *permission,
							      int argc,
							      char *argv[])
{
	static char unknown[] = "";
	CommandEntry *cmd;

	current_command = unknown;

	if (argc == 0)
		return NULL;

	if (!findInList(commandList, argv[0], (void *)&cmd)) {
		if (fd) {
			commandError(fd, ACK_ERROR_UNKNOWN,
				     "unknown command \"%s\"", argv[0]);
		}
		return NULL;
	}

	current_command = cmd->cmd;

	if (checkArgcAndPermission(cmd, fd, *permission, argc, argv) < 0) {
		return NULL;
	}

	return cmd;
}

static CommandEntry *getCommandEntryFromString(char *string, int *permission)
{
	CommandEntry *cmd = NULL;
	char *argv[COMMAND_ARGV_MAX] = { NULL };
	int argc = buffer2array(string, argv, COMMAND_ARGV_MAX);

	if (0 == argc)
		return NULL;

	cmd = getCommandEntryAndCheckArgcAndPermission(0, permission,
						       argc, argv);

	return cmd;
}

static int processCommandInternal(int fd, int *permission,
				  char *commandString, struct strnode *cmdnode)
{
	int argc;
	char *argv[COMMAND_ARGV_MAX] = { NULL };
	CommandEntry *cmd;
	int ret = -1;

	argc = buffer2array(commandString, argv, COMMAND_ARGV_MAX);

	if (argc == 0)
		return 0;

	if ((cmd = getCommandEntryAndCheckArgcAndPermission(fd, permission,
							    argc, argv))) {
		if (!cmdnode || !cmd->listHandler) {
			ret = cmd->handler(fd, permission, argc, argv);
		} else {
			ret = cmd->listHandler(fd, permission, argc, argv,
					       cmdnode, cmd);
		}
	}

	current_command = NULL;

	return ret;
}

int processListOfCommands(int fd, int *permission, int *expired,
			  int listOK, struct strnode *list)
{
	struct strnode *cur = list;
	int ret = 0;

	command_listNum = 0;

	while (cur) {
		DEBUG("processListOfCommands: process command \"%s\"\n",
		      cur->data);
		ret = processCommandInternal(fd, permission, cur->data, cur);
		DEBUG("processListOfCommands: command returned %i\n", ret);
		if (ret != 0 || (*expired) != 0)
			goto out;
		else if (listOK)
			fdprintf(fd, "list_OK\n");
		command_listNum++;
		cur = cur->next;
	}
out:
	command_listNum = 0;
	return ret;
}

int processCommand(int fd, int *permission, char *commandString)
{
	return processCommandInternal(fd, permission, commandString, NULL);
}

mpd_fprintf_ void commandError(int fd, int error, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	if (current_command && fd != STDERR_FILENO) {
		fdprintf(fd, "ACK [%i@%i] {%s} ",
		         (int)error, command_listNum, current_command);
		vfdprintf(fd, fmt, args);
		fdprintf(fd, "\n");
		current_command = NULL;
	} else {
		fdprintf(STDERR_FILENO, "ACK [%i@%i] ",
		         (int)error, command_listNum);
		vfdprintf(STDERR_FILENO, fmt, args);
		fdprintf(STDERR_FILENO, "\n");
	}

	va_end(args);
}
