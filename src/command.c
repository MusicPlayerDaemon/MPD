/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
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
#include "tables.h"
#include "volume.h"
#include "path.h"
#include "stats.h"
#include "myfprintf.h"
#include "list.h"
#include "conf.h"
#include "permission.h"
#include "audio.h"
#include "buffer2array.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define COMMAND_PLAY            "play"
#define COMMAND_STOP            "stop"
#define COMMAND_PAUSE           "pause"
#define COMMAND_STATUS          "status"
#define COMMAND_KILL            "kill"
#define COMMAND_CLOSE           "close"
#define COMMAND_ADD             "add"
#define COMMAND_DELETE          "delete"
#define COMMAND_PLAYLIST        "playlist"
#define COMMAND_SHUFFLE         "shuffle"
#define COMMAND_CLEAR           "clear"
#define COMMAND_SAVE            "save"
#define COMMAND_LOAD            "load"
#define COMMAND_LSINFO          "lsinfo"
#define COMMAND_RM              "rm"
#define COMMAND_PLAYLISTINFO    "playlistinfo"
#define COMMAND_FIND            "find"
#define COMMAND_SEARCH          "search"
#define COMMAND_UPDATE          "update"
#define COMMAND_NEXT            "next"
#define COMMAND_PREVIOUS        "previous"
#define COMMAND_LISTALL         "listall"
#define COMMAND_VOLUME          "volume"
#define COMMAND_REPEAT          "repeat"
#define COMMAND_RANDOM          "random"
#define COMMAND_STATS           "stats"
#define COMMAND_CLEAR_ERROR     "clearerror"
#define COMMAND_LIST            "list"
#define COMMAND_MOVE            "move"
#define COMMAND_SWAP            "swap"
#define COMMAND_SEEK            "seek"
#define COMMAND_LISTALLINFO	"listallinfo"
#define COMMAND_PING		"ping"
#define COMMAND_SETVOL		"setvol"
#define COMMAND_PASSWORD	"password"
#define COMMAND_CROSSFADE	"crossfade"
#define COMMAND_URL_HANDLERS    "urlhandlers" 

#define COMMAND_STATUS_VOLUME           "volume"
#define COMMAND_STATUS_STATE            "state"
#define COMMAND_STATUS_REPEAT           "repeat"
#define COMMAND_STATUS_RANDOM           "random"
#define COMMAND_STATUS_PLAYLIST         "playlist"
#define COMMAND_STATUS_PLAYLIST_LENGTH  "playlistlength"
#define COMMAND_STATUS_SONG             "song"
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

CommandEntry * getCommandEntryFromString(char * string, int * permission);

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

void addCommand(char * name, unsigned int reqPermission, int minargs,
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

int handleUrlHandlers(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return printRemoteUrlHandlers(fp);
}

int handlePlay(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int song = -1;
        char * test;

        if(argArrayLength==2) {
                song = strtol(argArray[1],&test,10);
                if(*test!='\0') {
                        commandError(fp, "need a positive integer");
                        return -1;
                }
        }
        return playPlaylist(fp,song,0);
}

int handleStop(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return stopPlaylist(fp);
}

int handlePause(FILE * fp, unsigned int * permission, 
		int argArrayLength, char ** argArray) 
{
        if(argArrayLength==2) {
		char * test;
                int pause = strtol(argArray[1],&test,10);
                if(*test!='\0' || (pause!=0 && pause!=1)) {
                        commandError(fp, "\%s\" is not 0 or 1", argArray[1]);
                        return -1;
                }
		return playerSetPause(fp,pause);
        }
        return playerPause(fp);
}

int commandStatus(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        char * state = NULL;
	int updateJobId;
        int song;

        playPlaylistIfPlayerStopped();
        switch(getPlayerState()) {
                case PLAYER_STATE_STOP:
                        state = strdup(COMMAND_STOP);
                        break;
                case PLAYER_STATE_PAUSE:
                        state = strdup(COMMAND_PAUSE);
                        break;
                case PLAYER_STATE_PLAY:
                        state = strdup(COMMAND_PLAY);
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
        if(song >= 0) myfprintf(fp,"%s: %i\n",COMMAND_STATUS_SONG,song);
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

        free(state);

        return 0;
}

int handleKill(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return COMMAND_RETURN_KILL;
}

int handleClose(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return COMMAND_RETURN_CLOSE;
}

int handleAdd(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        char * path = NULL;

        if(argArrayLength == 2) {
		path = argArray[1];
		if(isRemoteUrl(path)) return addToPlaylist(fp,path);
	}
        return addAllIn(fp,path);
}

int handleDelete(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int song;
        char * test;

        song = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, "need a positive integer");
                return -1;
        }
        return deleteFromPlaylist(fp,song); 
}

int handlePlaylist(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return showPlaylist(fp);
}

int handleShuffle(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return shufflePlaylist(fp);
}

int handleClear(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return clearPlaylist(fp);
}

int handleSave(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return savePlaylist(fp,argArray[1]);
}

int handleLoad(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return loadPlaylist(fp,argArray[1]);
}

int handleLsInfo(FILE * fp, unsigned int * permission, int argArrayLength, 
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

int handleRm(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return deletePlaylist(fp,argArray[1]);
}

int handlePlaylistInfo(FILE * fp, unsigned int * permission, 
		int argArrayLength, char ** argArray) 
{
        int song = -1;
        char * test;

        if(argArrayLength == 2) {
                song = strtol(argArray[1],&test,10);
                if(*test!='\0') {
                        commandError(fp, "%s need a positive integer");
                        return -1;
                }
        }
        return playlistInfo(fp,song);
}

int handleFind(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return findSongsIn(fp,NULL,argArray[1],argArray[2]);
}

int handleSearch(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return searchForSongsIn(fp,NULL,argArray[1],argArray[2]);
}

int listHandleUpdate(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray, ListNode * commandNode, CommandEntry * cmd) 
{
	static List * pathList = NULL;
	CommandEntry * nextCmd = NULL;
	ListNode * nextNode = commandNode->nextNode;;

	if(!pathList) pathList = makeList(NULL);

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

int handleUpdate(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
	if(argArrayLength==2) {
		int ret;
		List * pathList = makeList(NULL);
		insertInList(pathList,argArray[1],NULL);
		ret = updateInit(fp,pathList);
		freeList(pathList);
		return ret;
	}
        return updateInit(fp,NULL);
}

int handleNext(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return nextSongInPlaylist(fp);
}

int handlePrevious(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return previousSongInPlaylist(fp);
}

int handleListAll(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        char * directory = NULL;

        if(argArrayLength==2) directory = argArray[1];
        return printAllIn(fp,directory);
}

int handleVolume(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int change;
        char * test;

        change = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, "need an integer");
                return -1;
        }
        return changeVolumeLevel(fp,change,1);
}

int handleSetVol(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int level;
        char * test;

        level = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, "need an integer");
                return -1;
        }
        return changeVolumeLevel(fp,level,0);
}

int handleRepeat(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int status;
        char * test;

        status = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, "need an integer");
                return -1;
        }
        return setPlaylistRepeatStatus(fp,status);
}

int handleRandom(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int status;
        char * test;

        status = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, "need an integer");
                return -1;
        }
        return setPlaylistRandomStatus(fp,status);
}

int handleStats(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return printStats(fp);
}

int handleClearError(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        clearPlayerError();
        return 0;
}

int handleList(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        char * arg1 = NULL;

        if(argArrayLength==3) arg1 = argArray[2];
        return printAllKeysOfTable(fp,argArray[1],arg1);
}

int handleMove(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int from;
        int to;
        char * test;

        from = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, "\"%s\" is not a integer", argArray[1]);
                return -1;
        }
        to = strtol(argArray[2],&test,10);
        if(*test!='\0') {
                commandError(fp, "\"%s\" is not a integer", argArray[2]);
                return -1;
        }
        return moveSongInPlaylist(fp,from,to);
}

int handleSwap(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int song1;
        int song2;
        char * test;

        song1 = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, "\"%s\" is not a integer", argArray[1]);
                return -1;
        }
        song2 = strtol(argArray[2],&test,10);
        if(*test!='\0') {
                commandError(fp, "\"%s\" is not a integer", argArray[2]);
                return -1;
        }
        return swapSongsInPlaylist(fp,song1,song2);
}

int handleSeek(FILE * fp, unsigned int * permission, int argArrayLength,
		char ** argArray) 
{
        int song;
        int time;
        char * test;

        song = strtol(argArray[1],&test,10);
        if(*test!='\0') {
                commandError(fp, "\"%s\" is not a integer", argArray[1]);
                return -1;
        }
        time = strtol(argArray[2],&test,10);
        if(*test!='\0') {
                commandError(fp, "\"%s\" is not a integer", argArray[2]);
                return -1;
        }
        return seekSongInPlaylist(fp,song,time);
}

int handleListAllInfo(FILE * fp, unsigned int * permission, int argArrayLength,
		char ** argArray) 
{
        char * directory = NULL;

        if(argArrayLength==2) directory = argArray[1];
        return printInfoForAllIn(fp,directory);
}

int handlePing(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        return 0;
}

int handlePassword(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
	if(getPermissionFromPassword(argArray[1],permission)<0) {
		commandError(fp, "incorrect password");
		return -1;
	}

	return 0;
}

int handleCrossfade(FILE * fp, unsigned int * permission, int argArrayLength, 
		char ** argArray) 
{
        int time;
        char * test;

        time = strtol(argArray[1],&test,10);
        if(*test!='\0' || time<0) {
                commandError(fp, "\"%s\" is not a integer >= 0", argArray[1]);
                return -1;
        }

	setPlayerCrossFade(time);

	return 0;
}

void initCommands() {
        commandList = makeList(free);

        addCommand(COMMAND_PLAY        ,PERMISSION_CONTROL, 0, 1,handlePlay,NULL);
        addCommand(COMMAND_STOP        ,PERMISSION_CONTROL, 0, 0,handleStop,NULL);
        addCommand(COMMAND_PAUSE       ,PERMISSION_CONTROL, 0, 1,handlePause,NULL);
        addCommand(COMMAND_STATUS      ,PERMISSION_READ,    0, 0,commandStatus,NULL);
        addCommand(COMMAND_KILL        ,PERMISSION_ADMIN,  -1,-1,handleKill,NULL);
        addCommand(COMMAND_CLOSE       ,0,                 -1,-1,handleClose,NULL);
        addCommand(COMMAND_ADD         ,PERMISSION_ADD,     0, 1,handleAdd,NULL);
        addCommand(COMMAND_DELETE      ,PERMISSION_CONTROL, 1, 1,handleDelete,NULL);
        addCommand(COMMAND_PLAYLIST    ,PERMISSION_READ,    0, 0,handlePlaylist,NULL);
        addCommand(COMMAND_SHUFFLE     ,PERMISSION_CONTROL, 0, 0,handleShuffle,NULL);
        addCommand(COMMAND_CLEAR       ,PERMISSION_CONTROL, 0, 0,handleClear,NULL);
        addCommand(COMMAND_SAVE        ,PERMISSION_CONTROL, 1, 1,handleSave,NULL);
        addCommand(COMMAND_LOAD        ,PERMISSION_ADD,     1, 1,handleLoad,NULL);
        addCommand(COMMAND_LSINFO      ,PERMISSION_READ,    0, 1,handleLsInfo,NULL);
        addCommand(COMMAND_RM          ,PERMISSION_CONTROL, 1, 1,handleRm,NULL);
        addCommand(COMMAND_PLAYLISTINFO,PERMISSION_READ,    0, 1,handlePlaylistInfo,NULL);
        addCommand(COMMAND_FIND        ,PERMISSION_READ,    2, 2,handleFind,NULL);
        addCommand(COMMAND_SEARCH      ,PERMISSION_READ,    2, 2,handleSearch,NULL);
        addCommand(COMMAND_UPDATE      ,PERMISSION_ADMIN,   0, 1,handleUpdate,listHandleUpdate);
        addCommand(COMMAND_NEXT        ,PERMISSION_CONTROL, 0, 0,handleNext,NULL);
        addCommand(COMMAND_PREVIOUS    ,PERMISSION_CONTROL, 0, 0,handlePrevious,NULL);
        addCommand(COMMAND_LISTALL     ,PERMISSION_READ,    0, 1,handleListAll,NULL);
        addCommand(COMMAND_VOLUME      ,PERMISSION_CONTROL, 1, 1,handleVolume,NULL);
        addCommand(COMMAND_REPEAT      ,PERMISSION_CONTROL, 1, 1,handleRepeat,NULL);
        addCommand(COMMAND_RANDOM      ,PERMISSION_CONTROL, 1, 1,handleRandom,NULL);
        addCommand(COMMAND_STATS       ,PERMISSION_READ,    0, 0,handleStats,NULL);
        addCommand(COMMAND_CLEAR_ERROR ,PERMISSION_CONTROL, 0, 0,handleClearError,NULL);
        addCommand(COMMAND_LIST        ,PERMISSION_READ,    1, 2,handleList,NULL);
        addCommand(COMMAND_MOVE        ,PERMISSION_CONTROL, 2, 2,handleMove,NULL);
        addCommand(COMMAND_SWAP        ,PERMISSION_CONTROL, 2, 2,handleSwap,NULL);
        addCommand(COMMAND_SEEK        ,PERMISSION_CONTROL, 2, 2,handleSeek,NULL);
        addCommand(COMMAND_LISTALLINFO ,PERMISSION_READ,    0, 1,handleListAllInfo,NULL);
        addCommand(COMMAND_PING        ,0,                  0, 0,handlePing,NULL);
        addCommand(COMMAND_SETVOL      ,PERMISSION_CONTROL, 1, 1,handleSetVol,NULL);
        addCommand(COMMAND_PASSWORD    ,0,                  1, 1,handlePassword,NULL);
        addCommand(COMMAND_CROSSFADE   ,PERMISSION_CONTROL, 1, 1,handleCrossfade,NULL);
        addCommand(COMMAND_URL_HANDLERS,PERMISSION_READ,    0, 0,handleUrlHandlers,NULL);

        sortList(commandList);
}

void finishCommands() {
        freeList(commandList);
}

int checkArgcAndPermission(CommandEntry * cmd, FILE *fp, 
		unsigned int permission, int argc, char** argArray)
{
        int min = cmd->min + 1;
        int max = cmd->max + 1;

	if (cmd->reqPermission != (permission & cmd->reqPermission)) {
		if(fp) {
                	commandError(fp, "you don't have permission for \"%s\"",
					cmd->cmd);
		}
                return -1;
	}

        if (min == 0) return 0;

        if (min == max && max != argc) {
                if(fp) {
			commandError(fp, "wrong number of arguments for \"%s\"",
					argArray[0]);
		}
                return -1;
        }
        else if (argc < min) {
                if(fp) {
                	commandError(fp, "too few arguments for \"%s\"",
					argArray[0]);
		}
                return -1;
        }
        else if (argc > max && max /* != 0 */) {
		if(fp) {
                	commandError(fp, "too many arguments for \"%s\"",
					argArray[0]);
		}
                return -1;
        }
        else return 0;
}

CommandEntry * getCommandEntryAndCheckArgcAndPermission(FILE * fp, 
		unsigned int * permission, int argArrayLength, char ** argArray)
{
        CommandEntry * cmd;

        if(argArrayLength == 0) return NULL;

        if(!findInList(commandList, argArray[0],(void *)&cmd)) {
		if(fp) commandError(fp, "unknown command \"%s\"", argArray[0]);
                return NULL;
        }

        if(checkArgcAndPermission(cmd, fp, *permission, argArrayLength, 
			argArray) < 0) 
	{
		return NULL;
	}

	return cmd;
}

CommandEntry * getCommandEntryFromString(char * string, int * permission) {
	CommandEntry * cmd = NULL;
	char ** argArray;
	int argArrayLength = buffer2array(string,&argArray);

	if(0==argArrayLength) return NULL;

	cmd = getCommandEntryAndCheckArgcAndPermission(NULL,permission,
			argArrayLength,argArray);
	freeArgArray(argArray,argArrayLength);

	return cmd;
}

int processCommandInternal(FILE * fp, unsigned int * permission, 
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

	return ret;
}

int proccessListOfCommands(FILE * fp, int * permission, int * expired, 
		List * list) 
{
	ListNode * node = list->firstNode;
	ListNode * tempNode;
	int ret = 0;

	while(node!=NULL) {
		DEBUG("proccesListOfCommands: process command \"%s\"\n",
				node->data);
		ret = processCommandInternal(fp,permission,(char *)node->data,
				node);
		DEBUG("proccessListOfCommands: command returned %i\n",ret);
		tempNode = node->nextNode;
		deleteNodeFromList(list,node);
		node = tempNode;
		if(ret!=0 || (*expired)!=0) node = NULL;
	}
	
	return ret;
}

int processCommand(FILE * fp, unsigned int * permission, char * commandString) {
	return processCommandInternal(fp,permission,commandString,NULL);
}
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
