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

#include "directory.h"

#include "ls.h"
#include "command.h"
#include "utils.h"
#include "path.h"
#include "log.h"
#include "conf.h"
#include "stats.h"
#include "playlist.h"
#include "listen.h"
#include "interface.h"
#include "volume.h"
#include "mpd_types.h"
#include "sig_handlers.h"
#include "player.h"

#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#define DIRECTORY_DIR		"directory: "
#define DIRECTORY_MTIME		"mtime: "
#define DIRECTORY_BEGIN		"begin: "
#define DIRECTORY_END		"end: "
#define DIRECTORY_INFO_BEGIN	"info_begin"
#define DIRECTORY_INFO_END	"info_end"
#define DIRECTORY_MPD_VERSION	"mpd_version: "
#define DIRECTORY_FS_CHARSET	"fs_charset: "

#define DIRECTORY_SEARCH_ALBUM		"album"
#define DIRECTORY_SEARCH_ARTIST		"artist"
#define DIRECTORY_SEARCH_TITLE		"title"
#define DIRECTORY_SEARCH_FILENAME	"filename"

#define DIRECTORY_UPDATE_EXIT_NOUPDATE  0
#define DIRECTORY_UPDATE_EXIT_UPDATE    1
#define DIRECTORY_UPDATE_EXIT_ERROR     2

#define DIRECTORY_RETURN_NOUPDATE       0
#define DIRECTORY_RETURN_UPDATE         1
#define DIRECTORY_RETURN_ERROR         -1

typedef List DirectoryList;

typedef struct _Directory {
	char * utf8name;
	DirectoryList * subDirectories;
	SongList * songs;
} Directory;

Directory * mp3rootDirectory = NULL;

char * directory_db;

time_t directory_dbModTime = 0;

volatile int directory_updatePid = 0;

volatile int directory_reReadDB = 0;

volatile mpd_uint16 directory_updateJobId = 0;

DirectoryList * newDirectoryList();

int addToDirectory(Directory * directory, char * shortname, char * name);

void freeDirectoryList(DirectoryList * list);

void freeDirectory(Directory * directory);

int exploreDirectory(Directory * directory);

int updateDirectory(Directory * directory);

void deleteEmptyDirectoriesInDirectory(Directory * directory);

void removeSongFromDirectory(Directory * directory, char * shortname);

int addSubDirectoryToDirectory(Directory * directory, char * shortname, 
				char * name);

Directory * getDirectoryDetails(char * name, char ** shortname, 
				Directory ** parentDirectory);

Directory * getDirectory(char * name);

Song * getSongDetails(char * file, char ** shortnameRet, 
		Directory ** directoryRet);

int updatePath(char * utf8path);

void sortDirectory(Directory * directory);

void clearUpdatePid() {
	directory_updatePid = 0;
}

int isUpdatingDB() {
	if(directory_updatePid>0 || directory_reReadDB) {
		return directory_updateJobId;
	}
	return 0;
}

void directory_sigChldHandler(int pid, int status) {
	if(directory_updatePid==pid) {
		if(WIFSIGNALED(status) && WTERMSIG(status)!=SIGTERM) {
                        ERROR("update process died from a "
                                        "non-TERM signal: %i\n",
                                        WTERMSIG(status));
                }
		else if(!WIFSIGNALED(status)) {
			switch(WEXITSTATUS(status)) 
		        {
                        case DIRECTORY_UPDATE_EXIT_UPDATE:
			        directory_reReadDB = 1;
			        DEBUG("direcotry_sigChldHandler: "
					"updated db\n");
                        case DIRECTORY_UPDATE_EXIT_NOUPDATE:
			        DEBUG("direcotry_sigChldHandler: "
					"update exitted succesffully\n");
                                break;
                        default:
                                ERROR("error updating db\n");
                        }
		}
		clearUpdatePid();
	}
}

void readDirectoryDBIfUpdateIsFinished() {
	if(directory_reReadDB && 0==directory_updatePid) {
		DEBUG("readDirectoryDB since update finished successfully\n");
		readDirectoryDB();
		playlistVersionChange();
		directory_reReadDB = 0;
	}
}

int updateInit(FILE * fp, List * pathList) {
	if(directory_updatePid > 0) {
		commandError(fp, ACK_ERROR_UPDATE_ALREADY, "already updating");
		return -1;
	}

	/* need to block CHLD signal, cause it can exit before we
		even get a chance to assign directory_updatePID */
	blockSignals();	
	directory_updatePid = fork();
       	if(directory_updatePid==0) {
              	/* child */
                int dbUpdated = 0;
		clearPlayerPid();
	
		unblockSignals();

		finishSigHandlers();
               	close(listenSocket);
               	freeAllInterfaces();
               	finishPlaylist();
		finishVolume();

		if(pathList) {
			ListNode * node = pathList->firstNode;

			while(node) {
				switch(updatePath(node->key)) {
                                case 1:
                                        dbUpdated = 1;
                                        break;
                                case 0:
                                        break;
                                default:
                                        exit(DIRECTORY_UPDATE_EXIT_ERROR);
                                }
				node = node->nextNode;
			}
		}
		else {
                        if((dbUpdated = updateDirectory(mp3rootDirectory))<0) {
                                exit(DIRECTORY_UPDATE_EXIT_ERROR);
                        }
                }

                if(!dbUpdated) exit(DIRECTORY_UPDATE_EXIT_NOUPDATE);

		/* ignore signals since we don't want them to corrupt the db*/
		ignoreSignals();
		if(writeDirectoryDB()<0) {
			ERROR("problems writing music db file, \"%s\"\n",
					directory_db);
			exit(DIRECTORY_UPDATE_EXIT_ERROR);
		}
		exit(DIRECTORY_UPDATE_EXIT_UPDATE);
	}
	else if(directory_updatePid < 0) {
		unblockSignals();
		ERROR("updateInit: Problems forking()'ing\n");
		commandError(fp, ACK_ERROR_SYSTEM,
                                "problems trying to update");
		directory_updatePid = 0;
		return -1;
	}
	unblockSignals();

	directory_updateJobId++;
	if(directory_updateJobId > 1<<15) directory_updateJobId = 1;
	DEBUG("updateInit: fork()'d update child for update job id %i\n",
			(int)directory_updateJobId);
	myfprintf(fp,"updating_db: %i\n",(int)directory_updateJobId);

	return 0;
}

Directory * newDirectory(char * dirname) {
	Directory * directory;

	directory = malloc(sizeof(Directory));

	if(dirname!=NULL) directory->utf8name = strdup(dirname);
	else directory->utf8name = NULL;
	directory->subDirectories = newDirectoryList();
	directory->songs = newSongList();

	return directory;
}

void freeDirectory(Directory * directory) {
	freeDirectoryList(directory->subDirectories);
	freeSongList(directory->songs);
	if(directory->utf8name) free(directory->utf8name);
	free(directory);
}

DirectoryList * newDirectoryList() {
	return makeList((ListFreeDataFunc *)freeDirectory);
}

void freeDirectoryList(DirectoryList * directoryList) {
	freeList(directoryList);
}

void removeSongFromDirectory(Directory * directory, char * shortname) {
	void * song;
	
	if(findInList(directory->songs,shortname,&song)) {
		LOG("removing: %s\n",((Song *)song)->utf8url);
		deleteFromList(directory->songs,shortname);
	}
}

void deleteEmptyDirectoriesInDirectory(Directory * directory) {
	ListNode * node = directory->subDirectories->firstNode;
	ListNode * nextNode;
	Directory * subDir;

	while(node) {
		subDir = (Directory *)node->data;
		deleteEmptyDirectoriesInDirectory(subDir);
		nextNode = node->nextNode;
		if(subDir->subDirectories->numberOfNodes==0 &&
				subDir->songs->numberOfNodes==0) 
		{
			deleteNodeFromList(directory->subDirectories,node);
		}
		node = nextNode;
	}
}

/* return values:
   -1 -> error
    0 -> no error, but nothing updated
    1 -> no error, and stuff updated
 */
int updateInDirectory(Directory * directory, char * shortname, char * name) {
	time_t mtime;
	void * song;
	void * subDir;

	if(isMusic(name,&mtime)) {
		if(0==findInList(directory->songs,shortname,&song)) {
			addToDirectory(directory,shortname,name);
                        return DIRECTORY_RETURN_UPDATE;
		}
		else if(mtime!=((Song *)song)->mtime) {
			LOG("updating %s\n",name);
			if(updateSongInfo((Song *)song)<0) {
				removeSongFromDirectory(directory,shortname);
			}
                        return 1;
		}
	}
	else if(isDir(name)) {
		if(findInList(directory->subDirectories,shortname,(void **)&subDir)) {
			if(updateDirectory((Directory *)subDir)>0) return 1;
		}
		else {
                        return addSubDirectoryToDirectory(directory,shortname,
                                        name);
                }
	}

	return 0;
}

/* return values:
   -1 -> error
    0 -> no error, but nothing removed
    1 -> no error, and stuff removed
 */
int removeDeletedFromDirectory(Directory * directory) {
	DIR * dir;
	char cwd[2];
	struct dirent * ent;
	char * dirname = directory->utf8name;
	List * entList = makeList(free);
	void * name;
	char * s;
	char * utf8;
	ListNode * node;
	ListNode * tmpNode;
        int ret = 0;

	cwd[0] = '.';
	cwd[1] = '\0';
	if(dirname==NULL) dirname=cwd;

	if((dir = opendir(rmp2amp(utf8ToFsCharset(dirname))))==NULL) return -1;

	while((ent = readdir(dir))) {
		if(ent->d_name[0]=='.') continue; /* hide hidden stuff */
                if(strchr(ent->d_name, '\n')) continue;

		utf8 = fsCharsetToUtf8(ent->d_name);

		if(!utf8) continue;

		if(directory->utf8name) {
			s = malloc(strlen(directory->utf8name)+strlen(utf8)+2);
			sprintf(s,"%s/%s",directory->utf8name,utf8);
		}
		else s= strdup(utf8);
		insertInList(entList,utf8,s);
	}

	closedir(dir);

	node = directory->subDirectories->firstNode;
	while(node) {
		tmpNode = node->nextNode;
		if(findInList(entList,node->key,&name)) {
			if(!isDir((char *)name)) {
				LOG("removing directory: %s\n",(char*)name);
				deleteFromList(directory->subDirectories,
						node->key);
                                ret = 1;
			}
		}
		else {
			LOG("removing directory: ");
			if(directory->utf8name) LOG("%s/",directory->utf8name);
			LOG("%s\n",node->key);
			deleteFromList(directory->subDirectories,node->key);
                        ret = 1;
		}
		node = tmpNode;
	}

	node = directory->songs->firstNode;
	while(node) {
		tmpNode = node->nextNode;
		if(findInList(entList,node->key,(void **)&name)) {
			if(!isMusic(name,NULL)) {
				removeSongFromDirectory(directory,node->key);
                                ret = 1;
			}
		}
		else {
			removeSongFromDirectory(directory,node->key);
                        ret = 1;
		}
		node = tmpNode;
	}

	freeList(entList);

	return ret;
}

Directory * addDirectoryPathToDB(char * utf8path, char ** shortname) {
	char * parent;
	Directory * parentDirectory;
	void * directory;

	parent = strdup(parentPath(utf8path));

	if(strlen(parent)==0) parentDirectory = (void *)mp3rootDirectory;
	else parentDirectory = addDirectoryPathToDB(parent,shortname);

	*shortname = utf8path+strlen(parent);
	while(*(*shortname) && *(*shortname)=='/') (*shortname)++;

	if(!findInList(parentDirectory->subDirectories,*shortname, &directory))
	{
                directory = newDirectory(utf8path);
                insertInList(parentDirectory->subDirectories,*shortname,
                                directory);
	}

	/* if we're adding directory paths, make sure to delete filenames
           with potentially the same name*/
	removeSongFromDirectory(parentDirectory,*shortname);

	free(parent);

	return (Directory *)parentDirectory;
}

Directory * addParentPathToDB(char * utf8path, char ** shortname) {
	char * parent;
	Directory * parentDirectory;

	parent = strdup(parentPath(utf8path));

	if(strlen(parent)==0) parentDirectory = (void *)mp3rootDirectory;
	else parentDirectory = addDirectoryPathToDB(parent,shortname);

	*shortname = utf8path+strlen(parent);
	while(*(*shortname) && *(*shortname)=='/') (*shortname)++;

	free(parent);

	return (Directory *)parentDirectory;
}

/* return values:
   -1 -> error
    0 -> no error, but nothing updated
    1 -> no error, and stuff updated
 */
int updatePath(char * utf8path) {
	Directory * directory;
	Directory * parentDirectory;
	Song * song;
	char * shortname;
	char * path = sanitizePathDup(utf8path);
        time_t mtime;
        int ret = 0;

	if(NULL==path) return -1;

	/* if path is in the DB try to update it, or else delete it */
	if((directory = getDirectoryDetails(path,&shortname,
			&parentDirectory))) 
	{
		/* if this update directory is successfull, we are done */
		if((ret = updateDirectory(directory))>=0)
		{
			free(path);
			sortDirectory(directory);
			return ret;
		}
		/* we don't want to delete the root directory */
		else if(directory == mp3rootDirectory) {
			free(path);
			return 0;
		}
		/* if updateDirectory fials, means we should delete it */
		else {
			LOG("removing directory: %s\n",path);
			deleteFromList(parentDirectory->subDirectories,
					shortname);
                        ret = 1;
                        /* don't return, path maybe a song now*/
		}
	}
	else if((song = getSongDetails(path,&shortname,&parentDirectory))) {
		/* if this song update is successfull, we are done */
		if(song && isMusic(song->utf8url,&mtime)) {
			free(path);
                        if(song->mtime==mtime) return 0;
                        else if(updateSongInfo(song)==0) return 1;
                        else {
                                removeSongFromDirectory(parentDirectory,
                                                shortname);
                                return 1;
                        }
		}
		/* if updateDirectory fials, means we should delete it */
		else {
                        removeSongFromDirectory(parentDirectory,shortname);
                        ret = 1;
                        /* don't return, path maybe a directory now*/
                }
	}

	/* path not found in the db, see if it actually exists on the fs.
	 * Also, if by chance a directory was replaced by a file of the same
         * name or vice versa, we need to add it to the db
         */
	if(isDir(path) || isMusic(path,NULL)) {
		parentDirectory = addParentPathToDB(path,&shortname);
		if(addToDirectory(parentDirectory,shortname,path)>0) ret = 1;
	}

	free(path);

        return ret;
}

/* return values:
   -1 -> error
    0 -> no error, but nothing updated
    1 -> no error, and stuff updated
 */
int updateDirectory(Directory * directory) {
	DIR * dir;
	char cwd[2];
	struct dirent * ent;
	char * s;
	char * utf8;
	char * dirname = directory->utf8name;
        int ret = 0;

	cwd[0] = '.';
	cwd[1] = '\0';
	if(dirname==NULL) dirname=cwd;

	if(removeDeletedFromDirectory(directory)>0) ret = 1;

	if((dir = opendir(rmp2amp(utf8ToFsCharset(dirname))))==NULL) return -1;

	while((ent = readdir(dir))) {
		if(ent->d_name[0]=='.') continue; /* hide hidden stuff */
                if(strchr(ent->d_name, '\n')) continue;

		utf8 = fsCharsetToUtf8(ent->d_name);

		if(!utf8) continue;

		utf8 = strdup(utf8);

		if(directory->utf8name) {
			s = malloc(strlen(directory->utf8name)+strlen(utf8)+2);
			sprintf(s,"%s/%s",directory->utf8name,utf8);
		}
		else s = strdup(utf8);
		if(updateInDirectory(directory,utf8,s)>0) ret = 1;
		free(utf8);
		free(s);
	}
	
	closedir(dir);

	return ret;
}

/* return values:
   -1 -> error
    0 -> no error, but nothing found
    1 -> no error, and stuff found
 */
int exploreDirectory(Directory * directory) {
	DIR * dir;
	char cwd[2];
	struct dirent * ent;
	char * s;
	char * utf8;
	char * dirname = directory->utf8name;
        int ret = 0;

	cwd[0] = '.';
	cwd[1] = '\0';
	if(dirname==NULL) dirname=cwd;

	DEBUG("explore: attempting to opendir: %s\n",dirname);
	if((dir = opendir(rmp2amp(utf8ToFsCharset(dirname))))==NULL) return -1;

	DEBUG("explore: %s\n",dirname);
	while((ent = readdir(dir))) {
		if(ent->d_name[0]=='.') continue; /* hide hidden stuff */
                if(strchr(ent->d_name, '\n')) continue;

		utf8 = fsCharsetToUtf8(ent->d_name);

		if(!utf8) continue;

		utf8 = strdup(utf8);

		DEBUG("explore: found: %s (%s)\n",ent->d_name,utf8);

		if(directory->utf8name) {
			s = malloc(strlen(directory->utf8name)+strlen(utf8)+2);
			sprintf(s,"%s/%s",directory->utf8name,utf8);
		}
		else s = strdup(utf8);
		if(addToDirectory(directory,utf8,s)>0) ret = 1;
		free(utf8);
		free(s);
	}
	
	closedir(dir);

	return ret;
}

int addSubDirectoryToDirectory(Directory * directory, char * shortname, 
	char * name) 
{
	Directory * subDirectory = newDirectory(name);
	
	if(exploreDirectory(subDirectory)<1) {
                freeDirectory(subDirectory);
                return 0;
        }

	insertInList(directory->subDirectories,shortname,subDirectory);

	return 1;
}

int addToDirectory(Directory * directory, char * shortname, char * name) {
	if(isDir(name)) {
		return addSubDirectoryToDirectory(directory,shortname,name);
	}
	else if(isMusic(name,NULL)) {
		Song * song;
		song = addSongToList(directory->songs,shortname,name,
				SONG_TYPE_FILE);
		if(!song) return -1;
		LOG("added %s\n",name);
		return 1;
	}

	DEBUG("addToDirectory: %s is not a directory or music\n",name);

	return -1;
}

void closeMp3Directory() {
	freeDirectory(mp3rootDirectory);
}

Directory * findSubDirectory(Directory * directory,char * name) {
	void * subDirectory;
	char * dup = strdup(name);
	char * key;

	key = strtok(dup,"/");
	if(!key) {
		free(dup);
		return NULL;
	}
	
	if(findInList(directory->subDirectories,key,&subDirectory)) {
		free(dup);
		return (Directory *)subDirectory;
	}

	free(dup);
	return NULL;
}

Directory * getSubDirectory(Directory * directory, char * name, 
		char ** shortname, Directory ** parentDirectory) 
{
	Directory * subDirectory;
	int len;

	if(name==NULL || name[0]=='\0' || strcmp(name,"/")==0) {
		return directory;
	}

	if((subDirectory = findSubDirectory(directory,name))==NULL) return NULL;

	*shortname = name;
	*parentDirectory = directory;

	len = 0;
	while(name[len]!='/' && name[len]!='\0') len++;
	while(name[len]=='/') len++;

	return getSubDirectory(subDirectory,&(name[len]),shortname,
				parentDirectory);
}

Directory * getDirectoryDetails(char * name, char ** shortname, 
		Directory ** parentDirectory) 
{
	*shortname = NULL;
	*parentDirectory = NULL;

	return getSubDirectory(mp3rootDirectory,name,shortname,parentDirectory);
}

Directory * getDirectory(char * name) {
	char * shortname;
	Directory * parentDirectory;

	return getSubDirectory(mp3rootDirectory,name,&shortname,
				&parentDirectory);
}

int printDirectoryList(FILE * fp, DirectoryList * directoryList) {
	ListNode * node = directoryList->firstNode;
	Directory * directory;

	while(node!=NULL) {
		directory = (Directory *)node->data;
		myfprintf(fp,"%s%s\n",DIRECTORY_DIR,directory->utf8name);
		node = node->nextNode;
	}

	return 0;
}

int printDirectoryInfo(FILE * fp, char * name) {
	Directory * directory;
	
	if((directory = getDirectory(name))==NULL) {
		commandError(fp, ACK_ERROR_NO_EXIST, "directory not found");
		return -1;
	}

	printDirectoryList(fp,directory->subDirectories);
	printSongInfoFromList(fp,directory->songs);

	return 0;
}

void writeDirectoryInfo(FILE * fp, Directory * directory) {
	ListNode * node = (directory->subDirectories)->firstNode;
	Directory * subDirectory;

	if(directory->utf8name) {
		myfprintf(fp,"%s%s\n",DIRECTORY_BEGIN,directory->utf8name);
	}
			
	while(node!=NULL) {
		subDirectory = (Directory *)node->data;
		myfprintf(fp,"%s%s\n",DIRECTORY_DIR,node->key);
		writeDirectoryInfo(fp,subDirectory);
		node = node->nextNode;
	}

	writeSongInfoFromList(fp,directory->songs);

	if(directory->utf8name) {
		myfprintf(fp,"%s%s\n",DIRECTORY_END,directory->utf8name);
	}
}

void readDirectoryInfo(FILE * fp,Directory * directory) {
	char buffer[MAXPATHLEN*2];
	int bufferSize = MAXPATHLEN*2;
	char * key;
	Directory * subDirectory;
	char * name;
	int strcmpRet;
	ListNode * nextDirNode = directory->subDirectories->firstNode;
	ListNode * nodeTemp;

	while(myFgets(buffer,bufferSize,fp) && 0!=strncmp(DIRECTORY_END,buffer,strlen(DIRECTORY_END))) {
		if(0==strncmp(DIRECTORY_DIR,buffer,strlen(DIRECTORY_DIR))) {
			key = strdup(&(buffer[strlen(DIRECTORY_DIR)]));
			if(myFgets(buffer,bufferSize,fp)<0) {
				        ERROR("Error reading db, fgets\n");
				exit(EXIT_FAILURE);
			}
                        /* for compatibility with db's prior to 0.11 */
			if(0==strncmp(DIRECTORY_MTIME,buffer,
                                        strlen(DIRECTORY_MTIME))) 
                        {
			        if(myFgets(buffer,bufferSize,fp)<0) {
				        ERROR("Error reading db, fgets\n");
				        exit(EXIT_FAILURE);
			        }
                        }
			if(strncmp(DIRECTORY_BEGIN,buffer,strlen(DIRECTORY_BEGIN))) {
				ERROR("Error reading db at line: %s\n",buffer);
				exit(EXIT_FAILURE);
			}
			name = strdup(&(buffer[strlen(DIRECTORY_BEGIN)]));

			while(nextDirNode && (strcmpRet = 
					strcmp(key,nextDirNode->key)) > 0) {
				nodeTemp = nextDirNode->nextNode;
				deleteNodeFromList(directory->subDirectories,
						nextDirNode);
				nextDirNode = nodeTemp;
			}

			if(NULL==nextDirNode) {
				subDirectory = newDirectory(name);
				insertInList(directory->subDirectories,key,
						(void *)subDirectory);
			}
			else if(strcmpRet == 0) {
				subDirectory = (Directory *)nextDirNode->data;
				nextDirNode = nextDirNode->nextNode;
			}
			else {
				subDirectory = newDirectory(name);
				insertInListBeforeNode(
						directory->subDirectories,
						nextDirNode,
						key,
						(void *)subDirectory);
			}

			free(key);
			free(name);
			readDirectoryInfo(fp,subDirectory);
		}
		else if(0==strncmp(SONG_BEGIN,buffer,strlen(SONG_BEGIN))) {
			readSongInfoIntoList(fp,directory->songs);
		}
		else {
			ERROR("Unknown line in db: %s\n",buffer);
			exit(EXIT_FAILURE);
		}
	}

	while(nextDirNode) {
		nodeTemp = nextDirNode->nextNode;
		deleteNodeFromList(directory->subDirectories,nextDirNode);
		nextDirNode = nodeTemp;
	}
}

void sortDirectory(Directory * directory) {
	ListNode * node = directory->subDirectories->firstNode;
	Directory * subDir;
	
	sortList(directory->subDirectories);
	sortList(directory->songs);

	while(node!=NULL) {
		subDir = (Directory *)node->data;
		sortDirectory(subDir);
		node = node->nextNode;
	}
}

int writeDirectoryDB() {
	FILE * fp;

	deleteEmptyDirectoriesInDirectory(mp3rootDirectory);
	sortDirectory(mp3rootDirectory);

	while(!(fp=fopen(directory_db,"w")) && errno==EINTR);
	if(!fp) return -1;

	/* block signals when writing the db so we don't get a corrupted db*/
	myfprintf(fp,"%s\n",DIRECTORY_INFO_BEGIN);
	myfprintf(fp,"%s%s\n",DIRECTORY_MPD_VERSION,VERSION);
	myfprintf(fp,"%s%s\n",DIRECTORY_FS_CHARSET,getFsCharset());
	myfprintf(fp,"%s\n",DIRECTORY_INFO_END);

	writeDirectoryInfo(fp,mp3rootDirectory);

	while(fclose(fp) && errno==EINTR);

	return 0;
}

int readDirectoryDB() {
	FILE * fp;
        struct stat st;

	if(!mp3rootDirectory) mp3rootDirectory = newDirectory(NULL);
	while(!(fp=fopen(directory_db,"r")) && errno==EINTR);
	if(!fp) return -1;

	/* get initial info */
	{
		char buffer[100];
		int bufferSize = 100;
		int foundFsCharset = 0;
		int foundVersion = 0;

		if(myFgets(buffer,bufferSize,fp)<0) {
			ERROR("Error reading db, fgets\n");
			exit(EXIT_FAILURE);
		}
		if(0==strcmp(DIRECTORY_INFO_BEGIN,buffer)) {
			while(myFgets(buffer,bufferSize,fp) && 
					0!=strcmp(DIRECTORY_INFO_END,buffer)) 
			{
				if(0==strncmp(DIRECTORY_MPD_VERSION,buffer,
						strlen(DIRECTORY_MPD_VERSION)))
				{
					if(foundVersion) {
						ERROR("already found "
							"version in db\n");
						exit(EXIT_FAILURE);
					}
					foundVersion = 1;
				}
				else if(0==strncmp(DIRECTORY_FS_CHARSET,buffer,
						strlen(DIRECTORY_FS_CHARSET)))
				{
					char * fsCharset;
					char * tempCharset; 

					if(foundFsCharset) {
						ERROR("already found "
							"fs charset in db\n");
						exit(EXIT_FAILURE);
					}

					foundFsCharset = 1;

					fsCharset = &(buffer[strlen(
							DIRECTORY_FS_CHARSET)]);
					if((tempCharset = 
						getConf()[CONF_FS_CHARSET]) && 
						strcmp(fsCharset,tempCharset))
					{
						ERROR("Using \"%s\" for the "
							"filesystem charset "
							"instead of \"%s\"\n",
							fsCharset,tempCharset);
						ERROR("maybe you need to "
							"recreate the db?\n");
						setFsCharset(fsCharset);
					}
				}
				else {
					ERROR("directory: unknown line in db info: %s\n",
						buffer);
					exit(EXIT_FAILURE);
				}
			}
		}
		else {
			ERROR("db info not found in db file\n");
			ERROR("you should recreate the db using --create-db\n");
			fseek(fp,0,SEEK_SET);
		}
	}

	readDirectoryInfo(fp,mp3rootDirectory);
	while(fclose(fp) && errno==EINTR);

	stats.numberOfSongs = countSongsIn(stderr,NULL);
	stats.dbPlayTime = sumSongTimesIn(stderr,NULL);

	if(stat(directory_db,&st)==0) directory_dbModTime = st.st_mtime;

	return 0;
}

void updateMp3Directory() {
	switch(updateDirectory(mp3rootDirectory)) {
        case 0:
                /* nothing updated */
                return;
        case 1:
	        if(writeDirectoryDB()<0) {
		        ERROR("problems writing music db file, \"%s\"\n",
                                        directory_db);
                        exit(EXIT_FAILURE);
	        }
                /* something was updated and db should be written */
                break;
        default:
		ERROR("problems updating music db\n");
                exit(EXIT_FAILURE);
	}

	return;
}

int traverseAllInSubDirectory(FILE * fp, Directory * directory,
                                int (*forEachSong)(FILE *, Song *, void *),
                                int (*forEachDir)(FILE *, Directory *, void *),
				void * data)
{
        ListNode * node = directory->songs->firstNode;
        Song * song;
        Directory * dir;
        int errFlag = 0;

        if(forEachDir) {
                errFlag = forEachDir(fp,directory,data);
                if(errFlag) return errFlag;
        }

        if(forEachSong) {
                while(node!=NULL && !errFlag) {
                        song = (Song *)node->data;
                        errFlag = forEachSong(fp,song,data);
                        node = node->nextNode;
                }
                if(errFlag) return errFlag;
        }

        node = directory->subDirectories->firstNode;

        while(node!=NULL && !errFlag) {
                dir = (Directory *)node->data;
                errFlag = traverseAllInSubDirectory(fp,dir,forEachSong,
                                                        forEachDir,data);
                node = node->nextNode;
        }

        return errFlag;
}

int traverseAllIn(FILE * fp, char * name, 
			int (*forEachSong)(FILE *, Song *, void *),
			int (*forEachDir)(FILE *, Directory *, void *),
			void * data) {
	Directory * directory;

	if((directory = getDirectory(name))==NULL) {
		Song * song;
		if((song = getSongFromDB(name)) && forEachSong) {
			return forEachSong(fp, song, data);
		}
		commandError(fp, ACK_ERROR_NO_EXIST,
                                "directory or file not found");
		return -1;
	}

	return traverseAllInSubDirectory(fp,directory,forEachSong,forEachDir,
			data);
}

int countSongsInDirectory(FILE * fp, Directory * directory, void * data) {
	int * count = (int *)data;

	*count+=directory->songs->numberOfNodes;
	
        return 0;
}

int printDirectoryInDirectory(FILE * fp, Directory * directory, void * data) {
        if(directory->utf8name) {
		myfprintf(fp,"directory: %s\n",directory->utf8name);
	}
        return 0;
}

int printSongInDirectory(FILE * fp, Song * song, void * data) {
        myfprintf(fp,"file: %s\n",song->utf8url);
        return 0;
}

int searchForAlbumInDirectory(FILE * fp, Song * song, void * string) {
	if(song->tag && song->tag->album) {
		char * dup = strDupToUpper(song->tag->album);
		if(strstr(dup,(char *)string)) printSongInfo(fp,song);
		free(dup);
	}
	return 0;
}

int searchForArtistInDirectory(FILE * fp, Song * song, void * string) {
	if(song->tag && song->tag->artist) {
		char * dup = strDupToUpper(song->tag->artist);
		if(strstr(dup,(char *)string)) printSongInfo(fp,song);
		free(dup);
	}
	return 0;
}

int searchForTitleInDirectory(FILE * fp, Song * song, void * string) {
	if(song->tag && song->tag->title) {
		char * dup = strDupToUpper(song->tag->title);
		if(strstr(dup,(char *)string)) printSongInfo(fp,song);
		free(dup);
	}
	return 0;
}

int searchForFilenameInDirectory(FILE * fp, Song * song, void * string) {
	char * dup = strDupToUpper(song->utf8url);
	if(strstr(dup,(char *)string)) printSongInfo(fp,song);
	free(dup);
	return 0;
}

int searchForSongsIn(FILE * fp, char * name, char * item, char * string) {
	char * dup = strDupToUpper(string);
	int ret = -1;

	if(strcmp(item,DIRECTORY_SEARCH_ALBUM)==0) {
		ret = traverseAllIn(fp,name,searchForAlbumInDirectory,NULL,
			(void *)dup);
	}
	else if(strcmp(item,DIRECTORY_SEARCH_ARTIST)==0) {
		ret = traverseAllIn(fp,name,searchForArtistInDirectory,NULL,
			(void *)dup);
	}
	else if(strcmp(item,DIRECTORY_SEARCH_TITLE)==0) {
		ret = traverseAllIn(fp,name,searchForTitleInDirectory,NULL,
			(void *)dup);
	}
	else if(strcmp(item,DIRECTORY_SEARCH_FILENAME)==0) {
		ret = traverseAllIn(fp,name,searchForFilenameInDirectory,NULL,
			(void *)dup);
	}
	else commandError(fp, ACK_ERROR_ARG, "unknown table");

	free(dup);

	return ret;
}

int findAlbumInDirectory(FILE * fp, Song * song, void * string) {
	if(song->tag && song->tag->album && 
			strcmp((char *)string,song->tag->album)==0) 
	{
		printSongInfo(fp,song);
	}

	return 0;
}

int findArtistInDirectory(FILE * fp, Song * song, void * string) {
	if(song->tag && song->tag->artist && 
			strcmp((char *)string,song->tag->artist)==0) 
	{
		printSongInfo(fp,song);
	}

	return 0;
}

int findSongsIn(FILE * fp, char * name, char * item, char * string) {
	if(strcmp(item,DIRECTORY_SEARCH_ALBUM)==0) {
		return traverseAllIn(fp,name,findAlbumInDirectory,NULL,
			(void *)string);
	}
	else if(strcmp(item,DIRECTORY_SEARCH_ARTIST)==0) {
		return traverseAllIn(fp,name,findArtistInDirectory,NULL,
			(void *)string);
	}

	commandError(fp, ACK_ERROR_ARG, "unknown table");
	return -1;
}

int printAllIn(FILE * fp, char * name) {
	return traverseAllIn(fp,name,printSongInDirectory,
				printDirectoryInDirectory,NULL);
}

int directoryAddSongToPlaylist(FILE * fp, Song * song, void * data) {
	return addSongToPlaylist(fp,song);
}

int addAllIn(FILE * fp, char * name) {
	return traverseAllIn(fp,name,directoryAddSongToPlaylist,NULL,NULL);
}

int directoryPrintSongInfo(FILE * fp, Song * song, void * data) {
	return printSongInfo(fp,song);
}

int sumSongTime(FILE * fp, Song * song, void * data) {
	unsigned long * time = (unsigned long *)data;

	if(song->tag && song->tag->time>=0) *time+=song->tag->time;

	return 0;
}

int printInfoForAllIn(FILE * fp, char * name) {
        return traverseAllIn(fp,name,directoryPrintSongInfo,NULL,NULL);
}

int countSongsIn(FILE * fp, char * name) {
	int count = 0;
	void * ptr = (void *)&count;
	
        traverseAllIn(fp,name,NULL,countSongsInDirectory,ptr);

	return count;
}

unsigned long sumSongTimesIn(FILE * fp, char * name) {
	unsigned long dbPlayTime = 0;
	void * ptr = (void *)&dbPlayTime;
	
        traverseAllIn(fp,name,sumSongTime,NULL,ptr);

	return dbPlayTime;
}

void initMp3Directory() {
	struct stat st;

	mp3rootDirectory = newDirectory(NULL);
	exploreDirectory(mp3rootDirectory);
	stats.numberOfSongs = countSongsIn(stderr,NULL);
	stats.dbPlayTime = sumSongTimesIn(stderr,NULL);

	if(stat(directory_db,&st)==0) directory_dbModTime = st.st_mtime;
}

Song * getSongDetails(char * file, char ** shortnameRet, 
		Directory ** directoryRet)
{
	void * song = NULL;
	Directory * directory;
	char * dir = NULL;
	char * dup = strdup(file);
	char * shortname = dup;
	char * c = strtok(dup,"/");

	DEBUG("get song: %s\n",file);

	while(c) {
		shortname = c;
		c = strtok(NULL,"/");
	}

	if(shortname!=dup) {
		for(c = dup; c < shortname-1; c++) {
			if(*c=='\0') *c = '/';
		}
		dir = dup;
	}

	if(!(directory = getDirectory(dir))) {
		free(dup);
		return NULL;
	}

	if(!findInList(directory->songs,shortname,&song)) {
		free(dup);
		return NULL;
	}

	free(dup);
	if(shortnameRet) *shortnameRet = shortname;
	if(directoryRet) *directoryRet = directory;
	return (Song *)song;
}

Song * getSongFromDB(char * file) {
	return getSongDetails(file,NULL,NULL);
}

time_t getDbModTime() {
	return directory_dbModTime;
}
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
