/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (shank@mercury.chem.pitt.edu)
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
#include "dbUtils.h"

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

typedef struct _CommandEntry CommandEntry;

typedef int (* CommandHandlerFunction)(FILE *, unsigned int *, int, char **);
typedef int (* CommandListHandlerFunction)(FILE *, unsigned int *, int, char **,
		ListNode *, CommandEntry *);

/* if min: -1 don't check args *
 * if max: -1 no max args      */
struct _CommandEntry {
        char * cmd;
        int min;
        int max;
	unsigned int reqPermission;
        CommandHandlerFunction handler;
	CommandListHandlerFunction listHandler;
};

char * current_command = NULL;
int command_listNum = 0;

static CommandEntry * getCommandEntryFromString(char * string, int * permission);

List * commandList;

CommandEntry * newCommandEntry() {
        CommandEntry * cmd = malloc(sizeof(CommandEntry));
        cmd->cmd = NULL;
        cmd->min = 0;
        cmd->max = 0;
        cmd->handler = NULL;
        cmd->listHandler = NULL;
	cmd->reqPermission = 0;
        return cmd;
}

static void addCommand(char * name, unsigned int reqPermission, int minargs,
                int maxargs, CommandHandlerFunction handler_func,
		CommandListHandlerFunction listHandler_func) 
{
        CommandEntry * cmd = newCommandEntry();
        cmd->cmd = name;
        cmd->min = minargs;
        cmd->max = maxargs;
        cmd->handler = handler_func;
        cmd->listHandler = listHandler_func;
	cmd->reqPermission = reqPermission;

        insertInList(commandList, cmd->cmd, cmd);
}

static int handleUrlHandlers(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return printRemoteUrlHandlers(fp);
}

static int handlePlay(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int song = -1;
        char * test;

        if(argArrayLength==2) {
                song = strtol(argArray[1],&test,10);
                if(*test!='\0') {
                        commandError(fp, ACK_ERROR_ARG,
					 "need a positive integer", NULL);
                        return -1;
                }
        }
        return playPlaylist(fp,song,0);
}

static int handlePlayId(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int id = -1;
        char * test;

        if(argArrayLength==2) {
                id = strtol(argArray[1],&test,10);
                if(*test!='\0') {
                        commandError(fp, ACK_ERROR_ARG,
					 "need a positive integer", NULL);
                        return -1;
                }
        }
        return playPlaylistById(fp, id, 0);
}

static int handleStop(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return stopPlaylist(fp);
}

static int handleCurrentSong(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int song = getPlaylistCurrentSong();

	if(song >= 0) {
		return playlistInfo(fp, song);
	}
	else return 0;
}

static int handlePause(FILE * fp, unsigned int * permission, 
		int argArrayLength, char ** argArray) 
{
        if(argArrayLength==2) {
		char * test;
                int pause = strtol(argArray[1],&test,10);
                if(*test!='\0' || (pause!=0 && pause!=1)) {
                        commandError(fp, ACK_ERROR_ARG, "\%s\" is not 0 or 1", argArray[1]);
                        return -1;
                }
		return playerSetPause(fp,pause);
        }
        return playerPause(fp);
}

static int commandStatus(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        char * state = NULL;
	int updateJobId;
        int song;

        /*syncPlayerAndPlaylist();*/
	playPlaylistIfPlayerStopped();
        switch(getPlayerState()) {
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

        myfprintf(fp,"%s: %i\n",COMMAND_STATUS_VOLUME,getVolumeLevel());
        myfprintf(fp,"%s: %i\n",COMMAND_STATUS_REPEAT,getPlaylistRepeatStatus());
        myfprintf(fp,"%s: %i\n",COMMAND_STATUS_RANDOM,getPlaylistRandomStatus());
        myfprintf(fp,"%s: %li\n",COMMAND_STATUS_PLAYLIST,getPlaylistVersion());
        myfprintf(fp,"%s: %i\n",COMMAND_STATUS_PLAYLIST_LENGTH,getPlaylistLength());
	myfprintf(fp,"%s: %i\n",COMMAND_STATUS_CROSSFADE,
			(int)(getPlayerCrossFade()+0.5));

        myfprintf(fp,"%s: %s\n",COMMAND_STATUS_STATE,state);

        song = getPlaylistCurrentSong();
        if(song >= 0) {
		myfprintf(fp,"%s: %i\n", COMMAND_STATUS_SONG, song);
		myfprintf(fp,"%s: %i\n", COMMAND_STATUS_SONGID, 
				getPlaylistSongId(song));
	}
        if(getPlayerState()!=PLAYER_STATE_STOP) {
                myfprintf(fp,"%s: %i:%i\n",COMMAND_STATUS_TIME,getPlayerElapsedTime(),getPlayerTotalTime());
                myfprintf(fp,"%s: %li\n",COMMAND_STATUS_BITRATE,getPlayerBitRate(),getPlayerTotalTime());
                myfprintf(fp,"%s: %u:%i:%i\n",COMMAND_STATUS_AUDIO,getPlayerSampleRate(),getPlayerBits(),getPlayerChannels());
        }

	if((updateJobId = isUpdatingDB())) {
		myfprintf(fp,"%s: %i\n",COMMAND_STATUS_UPDATING_DB,updateJobId);
	}

        if(getPlayerError()!=PLAYER_ERROR_NOERROR) {
                myfprintf(fp,"%s: %s\n",COMMAND_STATUS_ERROR,getPlayerErrorStr());
        }

        return 0;
}

static int handleKill(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return COMMAND_RETURN_KILL;
}

static int handleClose(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return COMMAND_RETURN_CLOSE;
}

static int handleAdd(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        char * path = argArray[1];

	if(isRemoteUrl(path)) return addToPlaylist(fp, path, 0);

        return addAllIn(fp,path);
}

static int handleAddId(FILE * fp, unsigned int * permission, int argArrayLength,
		char ** argArray)
{
	return addToPlaylist(fp, argArray[1], 1);
}

static int handleDelete(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int song;
        char * test;

        song = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG, 
				"need a positive integer", NULL);
                return -1;
        }
        return deleteFromPlaylist(fp,song); 
}

static int handleDeleteId(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int id;
        char * test;

        id = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG, 
				"need a positive integer", NULL);
                return -1;
        }
        return deleteFromPlaylistById(fp, id); 
}

static int handlePlaylist(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return showPlaylist(fp);
}

static int handleShuffle(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return shufflePlaylist(fp);
}

static int handleClear(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return clearPlaylist(fp);
}

static int handleSave(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return savePlaylist(fp,argArray[1]);
}

static int handleLoad(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return loadPlaylist(fp,argArray[1]);
}

static int handleListPlaylist(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return PlaylistInfo(fp,argArray[1],0);
}

static int handleListPlaylistInfo(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return PlaylistInfo(fp,argArray[1], 1);
}

static int handleLsInfo(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        if(argArrayLength==1) {
                if(printDirectoryInfo(fp,NULL)<0) return -1;
                else return lsPlaylists(fp,"");
        }
        else {
                if(printDirectoryInfo(fp,argArray[1])<0) return -1;
                else return lsPlaylists(fp,argArray[1]);
        }
}

static int handleRm(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return deletePlaylist(fp,argArray[1]);
}

static int handlePlaylistChanges(FILE * fp, unsigned int * permission, 
		int argArrayLength, char ** argArray) 
{
        unsigned long version;
        char * test;

        version = strtoul(argArray[1], &test, 10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG, "need a positive integer", 
					NULL);
                return -1;
        }
        return playlistChanges(fp, version);
}

static int handlePlaylistChangesPosId(FILE * fp, unsigned int * permission, 
		int argArrayLength, char ** argArray) 
{
        unsigned long version;
        char * test;

        version = strtoul(argArray[1], &test, 10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG, "need a positive integer", 
					NULL);
                return -1;
        }
        return playlistChangesPosId(fp, version);
}

static int handlePlaylistInfo(FILE * fp, unsigned int * permission, 
		int argArrayLength, char ** argArray) 
{
        int song = -1;
        char * test;

        if(argArrayLength == 2) {
                song = strtol(argArray[1],&test,10);
                if(*test!='\0') {
                        commandError(fp, ACK_ERROR_ARG,
					"need a positive integer", NULL);
                        return -1;
                }
        }
        return playlistInfo(fp,song);
}

static int handlePlaylistId(FILE * fp, unsigned int * permission, 
		int argArrayLength, char ** argArray) 
{
        int id = -1;
        char * test;

        if(argArrayLength == 2) {
                id = strtol(argArray[1],&test,10);
                if(*test!='\0') {
                        commandError(fp, ACK_ERROR_ARG,
					"need a positive integer", NULL);
                        return -1;
                }
        }
        return playlistId(fp, id);
}

static int handleFind(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
	int ret;

	LocateTagItem * items;
	int numItems = newLocateTagItemArrayFromArgArray(argArray+1, 
			argArrayLength-1, &items);

	if(numItems <= 0) {
		commandError(fp, ACK_ERROR_ARG, "incorrect arguments", NULL); 
		return -1;
	}

        ret = findSongsIn(fp, NULL, numItems, items);

	freeLocateTagItemArray(numItems, items);

	return ret;
}

static int handleSearch(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
	int ret;

	LocateTagItem * items;
	int numItems = newLocateTagItemArrayFromArgArray(argArray+1, 
			argArrayLength-1, &items);

	if(numItems <= 0) {
		commandError(fp, ACK_ERROR_ARG, "incorrect arguments", NULL); 
		return -1;
	}

        ret = searchForSongsIn(fp, NULL, numItems, items);

	freeLocateTagItemArray(numItems, items);

	return ret;
}

static int listHandleUpdate(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray, ListNode * commandNode, CommandEntry * cmd) 
{
	static List * pathList = NULL;
	CommandEntry * nextCmd = NULL;
	ListNode * nextNode = commandNode->nextNode;;

	if(!pathList) pathList = makeList(NULL, 1);

	if(argArrayLength==2) insertInList(pathList,argArray[1],NULL);
	else insertInList(pathList,"",NULL);

	if(nextNode) {
		nextCmd = getCommandEntryFromString((void *)nextNode->data,
				permission);
	}

	if(cmd!=nextCmd) {
		int ret = updateInit(fp,pathList);
		freeList(pathList);
		pathList = NULL;
		return ret;
	}

	return 0;
}

static int handleUpdate(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
	if(argArrayLength==2) {
		int ret;
		List * pathList = makeList(NULL, 1);
		insertInList(pathList,argArray[1],NULL);
		ret = updateInit(fp,pathList);
		freeList(pathList);
		return ret;
	}
        return updateInit(fp,NULL);
}

static int handleNext(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return nextSongInPlaylist(fp);
}

static int handlePrevious(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return previousSongInPlaylist(fp);
}

static int handleListAll(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        char * directory = NULL;

        if(argArrayLength==2) directory = argArray[1];
        return printAllIn(fp,directory);
}

static int handleVolume(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int change;
        char * test;

        change = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG, "need an integer", NULL);
                return -1;
        }
        return changeVolumeLevel(fp,change,1);
}

static int handleSetVol(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int level;
        char * test;

        level = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG, "need an integer", NULL);
                return -1;
        }
        return changeVolumeLevel(fp,level,0);
}

static int handleRepeat(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int status;
        char * test;

        status = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG, "need an integer", NULL);
                return -1;
        }
        return setPlaylistRepeatStatus(fp,status);
}

static int handleRandom(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int status;
        char * test;

        status = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG, "need an integer", NULL);
                return -1;
        }
        return setPlaylistRandomStatus(fp,status);
}

static int handleStats(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return printStats(fp);
}

static int handleClearError(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        clearPlayerError();
        return 0;
}

static int handleList(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
	int numConditionals = 0;
	LocateTagItem * conditionals = NULL;
	int tagType = getLocateTagItemType(argArray[1]);
	int ret;

	if(tagType < 0) {
                commandError(fp, ACK_ERROR_ARG,
				"\"%s\" is not known", argArray[1]);
		return -1;
	}

	/* for compatibility with < 0.12.0 */
        if(argArrayLength==3) {
		if(tagType != TAG_ITEM_ALBUM) {
                	commandError(fp, ACK_ERROR_ARG,
					"should be \"%s\" for 3 arguments", 
					mpdTagItemKeys[TAG_ITEM_ALBUM]);
			return -1;
		}
		conditionals = newLocateTagItem(mpdTagItemKeys[TAG_ITEM_ARTIST],
					argArray[2]);
		numConditionals = 1;
	}
	else {
		numConditionals = newLocateTagItemArrayFromArgArray(argArray+2,
					argArrayLength-2, &conditionals);

		if(numConditionals < 0) {
                	commandError(fp, ACK_ERROR_ARG,
					"not able to parse args", NULL);
			return -1;
		}
	}

	ret = listAllUniqueTags(fp, tagType, numConditionals,conditionals);

	if(conditionals) freeLocateTagItemArray(numConditionals, conditionals);

	return ret;
}

static int handleMove(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int from;
        int to;
        char * test;

        from = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG,
				"\"%s\" is not a integer", argArray[1]);
                return -1;
        }
        to = strtol(argArray[2],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG,
				"\"%s\" is not a integer", argArray[2]);
                return -1;
        }
        return moveSongInPlaylist(fp,from,to);
}

static int handleMoveId(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int id;
        int to;
        char * test;

        id = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG,
				"\"%s\" is not a integer", argArray[1]);
                return -1;
        }
        to = strtol(argArray[2],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG,
				"\"%s\" is not a integer", argArray[2]);
                return -1;
        }
        return moveSongInPlaylistById(fp, id, to);
}

static int handleSwap(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int song1;
        int song2;
        char * test;

        song1 = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG,
				"\"%s\" is not a integer", argArray[1]);
                return -1;
        }
        song2 = strtol(argArray[2],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG, "\"%s\" is not a integer",
				argArray[2]);
                return -1;
        }
        return swapSongsInPlaylist(fp,song1,song2);
}

static int handleSwapId(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int id1;
        int id2;
        char * test;

        id1 = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG,
				"\"%s\" is not a integer", argArray[1]);
                return -1;
        }
        id2 = strtol(argArray[2],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG, "\"%s\" is not a integer",
				argArray[2]);
                return -1;
        }
        return swapSongsInPlaylistById(fp, id1, id2);
}

static int handleSeek(FILE * fp, unsigned int * permission, int argArrayLength,
		char ** argArray) 
{
        int song;
        int time;
        char * test;

        song = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG,
				"\"%s\" is not a integer", argArray[1]);
                return -1;
        }
        time = strtol(argArray[2],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG,
				"\"%s\" is not a integer", argArray[2]);
                return -1;
        }
        return seekSongInPlaylist(fp,song,time);
}

static int handleSeekId(FILE * fp, unsigned int * permission, int argArrayLength,
		char ** argArray) 
{
        int id;
        int time;
        char * test;

        id = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG,
				"\"%s\" is not a integer", argArray[1]);
                return -1;
        }
        time = strtol(argArray[2],&test,10);
        if(*test!='\0') {
                commandError(fp, ACK_ERROR_ARG,
				"\"%s\" is not a integer", argArray[2]);
                return -1;
        }
        return seekSongInPlaylistById(fp, id, time);
}

static int handleListAllInfo(FILE * fp, unsigned int * permission, int argArrayLength,
		char ** argArray) 
{
        char * directory = NULL;

        if(argArrayLength==2) directory = argArray[1];
        return printInfoForAllIn(fp,directory);
}

static int handlePing(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return 0;
}

static int handlePassword(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
	if(getPermissionFromPassword(argArray[1],permission)<0) {
		commandError(fp, ACK_ERROR_PASSWORD, "incorrect password", NULL);
		return -1;
	}

	return 0;
}

static int handleCrossfade(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int time;
        char * test;

        time = strtol(argArray[1],&test,10);
        if(*test!='\0' || time<0) {
                commandError(fp, ACK_ERROR_ARG, 
				"\"%s\" is not a integer >= 0", argArray[1]);
                return -1;
        }

	setPlayerCrossFade(time);

	return 0;
}

static int handleEnableDevice(FILE * fp, unsigned int * permission, int argArrayLength,
		char ** argArray) 
{
        int device;
        char * test;

        device = strtol(argArray[1],&test,10);
        if(*test!='\0' || device<0) {
                commandError(fp, ACK_ERROR_ARG, 
				"\"%s\" is not a integer >= 0", argArray[1]);
                return -1;
        }

	return enableAudioDevice(fp, device);
}

static int handleDisableDevice(FILE * fp, unsigned int * permission, 
		int argArrayLength, char ** argArray) 
{
        int device;
        char * test;

        device = strtol(argArray[1],&test,10);
        if(*test!='\0' || device<0) {
                commandError(fp, ACK_ERROR_ARG, 
				"\"%s\" is not a integer >= 0", argArray[1]);
                return -1;
        }

	return disableAudioDevice(fp, device);
}

static int handleDevices(FILE * fp, unsigned int * permission, int argArrayLength,
		char ** argArray)
{
	printAudioDevices(fp);

	return 0;
}

/* don't be fooled, this is the command handler for "commands" command */
static int handleCommands(FILE * fp, unsigned int * permission, int argArrayLength,
		char ** argArray)
{
	ListNode * node = commandList->firstNode;
	CommandEntry * cmd;

	while(node != NULL) {
		cmd = (CommandEntry *) node->data;
		if(cmd->reqPermission == (*permission & cmd->reqPermission)) {
			myfprintf(fp, "command: %s\n", cmd->cmd);
		}

		node = node->nextNode;
	}

	return 0;
}

static int handleNotcommands(FILE * fp, unsigned int * permission, int argArrayLength,
		char ** argArray)
{
	ListNode * node = commandList->firstNode;
	CommandEntry * cmd;

	while(node != NULL) {
		cmd = (CommandEntry *) node->data;

		if(cmd->reqPermission != (*permission & cmd->reqPermission)) {
			myfprintf(fp, "command: %s\n", cmd->cmd);
		}

		node = node->nextNode;
	}

	return 0;
}

void initCommands() {
        commandList = makeList(free, 1);

        addCommand(COMMAND_PLAY        ,PERMISSION_CONTROL, 0, 1,handlePlay,NULL);
        addCommand(COMMAND_PLAYID      ,PERMISSION_CONTROL, 0, 1,handlePlayId,NULL);
        addCommand(COMMAND_STOP        ,PERMISSION_CONTROL, 0, 0,handleStop,NULL);
        addCommand(COMMAND_CURRENTSONG ,PERMISSION_READ   , 0, 0,handleCurrentSong,NULL);
        addCommand(COMMAND_PAUSE       ,PERMISSION_CONTROL, 0, 1,handlePause,NULL);
        addCommand(COMMAND_STATUS      ,PERMISSION_READ,    0, 0,commandStatus,NULL);
        addCommand(COMMAND_KILL        ,PERMISSION_ADMIN,  -1,-1,handleKill,NULL);
        addCommand(COMMAND_CLOSE       ,0,                 -1,-1,handleClose,NULL);
        addCommand(COMMAND_ADD         ,PERMISSION_ADD,     1, 1,handleAdd,NULL);
        addCommand(COMMAND_ADDID       ,PERMISSION_ADD,     1, 1,handleAddId,NULL);
        addCommand(COMMAND_DELETE      ,PERMISSION_CONTROL, 1, 1,handleDelete,NULL);
        addCommand(COMMAND_DELETEID    ,PERMISSION_CONTROL, 1, 1,handleDeleteId,NULL);
        addCommand(COMMAND_PLAYLIST    ,PERMISSION_READ,    0, 0,handlePlaylist,NULL);
        addCommand(COMMAND_PLAYLISTID  ,PERMISSION_READ,    0, 1,handlePlaylistId,NULL);
        addCommand(COMMAND_SHUFFLE     ,PERMISSION_CONTROL, 0, 0,handleShuffle,NULL);
        addCommand(COMMAND_CLEAR       ,PERMISSION_CONTROL, 0, 0,handleClear,NULL);
        addCommand(COMMAND_SAVE        ,PERMISSION_CONTROL, 1, 1,handleSave,NULL);
        addCommand(COMMAND_LOAD        ,PERMISSION_ADD,     1, 1,handleLoad,NULL);
        addCommand(COMMAND_LISTPLAYLIST,PERMISSION_READ,     1, 1,handleListPlaylist,NULL);
        addCommand(COMMAND_LISTPLAYLISTINFO,PERMISSION_READ,     1, 1,handleListPlaylistInfo,NULL);
        addCommand(COMMAND_LSINFO      ,PERMISSION_READ,    0, 1,handleLsInfo,NULL);
        addCommand(COMMAND_RM          ,PERMISSION_CONTROL, 1, 1,handleRm,NULL);
        addCommand(COMMAND_PLAYLISTINFO,PERMISSION_READ,    0, 1,handlePlaylistInfo,NULL);
        addCommand(COMMAND_FIND        ,PERMISSION_READ,    2,-1,handleFind,NULL);
        addCommand(COMMAND_SEARCH      ,PERMISSION_READ,    2,-1,handleSearch,NULL);
        addCommand(COMMAND_UPDATE      ,PERMISSION_ADMIN,   0, 1,handleUpdate,listHandleUpdate);
        addCommand(COMMAND_NEXT        ,PERMISSION_CONTROL, 0, 0,handleNext,NULL);
        addCommand(COMMAND_PREVIOUS    ,PERMISSION_CONTROL, 0, 0,handlePrevious,NULL);
        addCommand(COMMAND_LISTALL     ,PERMISSION_READ,    0, 1,handleListAll,NULL);
        addCommand(COMMAND_VOLUME      ,PERMISSION_CONTROL, 1, 1,handleVolume,NULL);
        addCommand(COMMAND_REPEAT      ,PERMISSION_CONTROL, 1, 1,handleRepeat,NULL);
        addCommand(COMMAND_RANDOM      ,PERMISSION_CONTROL, 1, 1,handleRandom,NULL);
        addCommand(COMMAND_STATS       ,PERMISSION_READ,    0, 0,handleStats,NULL);
        addCommand(COMMAND_CLEAR_ERROR ,PERMISSION_CONTROL, 0, 0,handleClearError,NULL);
        addCommand(COMMAND_LIST        ,PERMISSION_READ,    1,-1,handleList,NULL);
        addCommand(COMMAND_MOVE        ,PERMISSION_CONTROL, 2, 2,handleMove,NULL);
        addCommand(COMMAND_MOVEID      ,PERMISSION_CONTROL, 2, 2,handleMoveId,NULL);
        addCommand(COMMAND_SWAP        ,PERMISSION_CONTROL, 2, 2,handleSwap,NULL);
        addCommand(COMMAND_SWAPID      ,PERMISSION_CONTROL, 2, 2,handleSwapId,NULL);
        addCommand(COMMAND_SEEK        ,PERMISSION_CONTROL, 2, 2,handleSeek,NULL);
        addCommand(COMMAND_SEEKID      ,PERMISSION_CONTROL, 2, 2,handleSeekId,NULL);
        addCommand(COMMAND_LISTALLINFO ,PERMISSION_READ,    0, 1,handleListAllInfo,NULL);
        addCommand(COMMAND_PING        ,0,                  0, 0,handlePing,NULL);
        addCommand(COMMAND_SETVOL      ,PERMISSION_CONTROL, 1, 1,handleSetVol,NULL);
        addCommand(COMMAND_PASSWORD    ,0,                  1, 1,handlePassword,NULL);
        addCommand(COMMAND_CROSSFADE   ,PERMISSION_CONTROL, 1, 1,handleCrossfade,NULL);
        addCommand(COMMAND_URL_HANDLERS,PERMISSION_READ,    0, 0,handleUrlHandlers,NULL);
        addCommand(COMMAND_PLCHANGES   ,PERMISSION_READ,    1, 1,handlePlaylistChanges,NULL);
        addCommand(COMMAND_PLCHANGESPOSID   ,PERMISSION_READ,    1, 1,handlePlaylistChangesPosId,NULL);
        addCommand(COMMAND_ENABLE_DEV  ,PERMISSION_ADMIN,   1, 1,handleEnableDevice,NULL);
        addCommand(COMMAND_DISABLE_DEV ,PERMISSION_ADMIN,   1, 1,handleDisableDevice,NULL);
        addCommand(COMMAND_DEVICES     ,PERMISSION_READ,   0, 0,handleDevices,NULL);
        addCommand(COMMAND_COMMANDS    ,0,                  0, 0,handleCommands,NULL);
        addCommand(COMMAND_NOTCOMMANDS ,0,                  0, 0,handleNotcommands,NULL);

        sortList(commandList);
}

void finishCommands() {
        freeList(commandList);
}

static int checkArgcAndPermission(CommandEntry * cmd, FILE *fp, 
		unsigned int permission, int argc, char** argArray)
{
        int min = cmd->min + 1;
        int max = cmd->max + 1;

	if (cmd->reqPermission != (permission & cmd->reqPermission)) {
		if(fp) {
                	commandError(fp, ACK_ERROR_PERMISSION,
					"you don't have permission for \"%s\"",
					cmd->cmd);
		}
                return -1;
	}

        if (min == 0) return 0;

        if (min == max && max != argc) {
                if(fp) {
			commandError(fp, ACK_ERROR_ARG, 
					"wrong number of arguments for \"%s\"",
					argArray[0]);
		}
                return -1;
        }
        else if (argc < min) {
                if(fp) {
                	commandError(fp, ACK_ERROR_ARG, 
					"too few arguments for \"%s\"",
					argArray[0]);
		}
                return -1;
        }
        else if (argc > max && max /* != 0 */) {
		if(fp) {
                	commandError(fp, ACK_ERROR_ARG,
					"too many arguments for \"%s\"",
					argArray[0]);
		}
                return -1;
        }
        else return 0;
}

static CommandEntry * getCommandEntryAndCheckArgcAndPermission(FILE * fp, 
		unsigned int * permission, int argArrayLength, char ** argArray)
{
	static char unknown[] = "";
        CommandEntry * cmd;

	current_command = unknown;

        if(argArrayLength == 0) return NULL;

        if(!findInList(commandList, argArray[0],(void *)&cmd)) {
		if(fp) {
			commandError(fp, ACK_ERROR_UNKNOWN,
					"unknown command \"%s\"", argArray[0]);
		}
                return NULL;
        }

	current_command = cmd->cmd;

        if(checkArgcAndPermission(cmd, fp, *permission, argArrayLength, 
			argArray) < 0) 
	{
		return NULL;
	}

	return cmd;
}

static CommandEntry * getCommandEntryFromString(char * string, int * permission) {
	CommandEntry * cmd = NULL;
	char ** argArray;
	int argArrayLength = buffer2array(string,&argArray);

	if(0==argArrayLength) return NULL;

	cmd = getCommandEntryAndCheckArgcAndPermission(NULL,permission,
			argArrayLength,argArray);
	freeArgArray(argArray,argArrayLength);

	return cmd;
}

static int processCommandInternal(FILE * fp, unsigned int * permission, 
		char * commandString,
		ListNode * commandNode) 
{
	int argArrayLength;
	char ** argArray;
        CommandEntry * cmd;
	int ret = -1;

	argArrayLength = buffer2array(commandString,&argArray);

        if(argArrayLength == 0) return 0;

	if((cmd = getCommandEntryAndCheckArgcAndPermission(fp,permission,
			argArrayLength,argArray))) 
	{
        	if(NULL==commandNode || NULL==cmd->listHandler) {
			ret = cmd->handler(fp, permission, argArrayLength, 
					argArray);
		}
		else {
			ret = cmd->listHandler(fp, permission, argArrayLength,
					argArray, commandNode, cmd);
		}
	}

	freeArgArray(argArray,argArrayLength);

	current_command = NULL;

	return ret;
}

int processListOfCommands(FILE * fp, int * permission, int * expired, 
		int listOK, List * list) 
{
	ListNode * node = list->firstNode;
	ListNode * tempNode;
	int ret = 0;

	command_listNum = 0;

	while(node!=NULL) {
		DEBUG("processListOfCommands: process command \"%s\"\n",
				node->data);
		ret = processCommandInternal(fp,permission,(char *)node->data,
				node);
		DEBUG("processListOfCommands: command returned %i\n",ret);
		tempNode = node->nextNode;
		deleteNodeFromList(list,node);
		node = tempNode;
		if(ret!=0 || (*expired)!=0) node = NULL;
		else if(listOK) myfprintf(fp, "list_OK\n");
		command_listNum++;
	}

	command_listNum = 0;
	
	return ret;
}

int processCommand(FILE * fp, unsigned int * permission, char * commandString) {
	return processCommandInternal(fp,permission,commandString,NULL);
}
