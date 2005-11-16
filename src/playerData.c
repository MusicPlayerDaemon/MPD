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

#include "playerData.h"
#include "conf.h"
#include "log.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int buffered_before_play;
int buffered_chunks;

#define DEFAULT_BUFFER_SIZE		2048
#define DEFAULT_BUFFER_BEFORE_PLAY	0

PlayerData * playerData_pd;
int * player_pid;

void initPlayerData() {
	float perc = DEFAULT_BUFFER_BEFORE_PLAY;
	char * test;
	int shmid;
	int crossfade = 0;
	size_t bufferSize = DEFAULT_BUFFER_SIZE;
	size_t allocationSize;
	OutputBuffer * buffer;
	ConfigParam * param;

	param = getConfigParam(CONF_AUDIO_BUFFER_SIZE);

	if(param) {
		bufferSize = strtol(param->value, &test, 10);
		if(*test!='\0' || bufferSize<=0) {
			ERROR("buffer size \"%s\" is not a positive integer, "
					"line %i\n", param->value, param->line);
			exit(EXIT_FAILURE);
		}
	}

	bufferSize*=1024;

	buffered_chunks = bufferSize/CHUNK_SIZE;

	if(buffered_chunks >= 1<<15) {
		ERROR("buffer size \"%i\" is too big\n",bufferSize);
		exit(EXIT_FAILURE);
	}

	param = getConfigParam(CONF_BUFFER_BEFORE_PLAY);

	if(param) {
		perc = strtod(param->value, &test);
		if(*test!='%' || perc<0 || perc>100) {
			ERROR("buffered before play \"%s\" is not a positive "
				"percentage and less than 100 percent, line %i"
				"\n", param->value, param->line);
			exit(EXIT_FAILURE);
		}
	}

	buffered_before_play = (perc/100)*buffered_chunks;
	if(buffered_before_play>buffered_chunks) {
		buffered_before_play = buffered_chunks;
	}
	else if(buffered_before_play<0) buffered_before_play = 0;

	allocationSize = buffered_chunks*CHUNK_SIZE; /*actual buffer*/
	allocationSize+= buffered_chunks*sizeof(float); /*for times*/
	allocationSize+= buffered_chunks*sizeof(mpd_sint16); /*for chunkSize*/
	allocationSize+= buffered_chunks*sizeof(mpd_sint16); /*for bitRate*/
	allocationSize+= buffered_chunks*sizeof(mpd_sint8); /*for metaChunk*/
	allocationSize+= sizeof(PlayerData); /*for playerData struct*/

	if((shmid = shmget(IPC_PRIVATE,allocationSize,IPC_CREAT|0600))<0) {
		ERROR("problems shmget'ing\n");
		exit(EXIT_FAILURE);
	}
	if((playerData_pd = shmat(shmid,NULL,0))<0) {
		ERROR("problems shmat'ing\n");
		exit(EXIT_FAILURE);
	}
	if (shmctl(shmid, IPC_RMID, 0)<0) {
		ERROR("problems shmctl'ing\n");
		exit(EXIT_FAILURE);
	}
	/* maybe the following should be put in the same shm block as the previous
	 * or maybe even made a part of the playerData struct
	 */
	allocationSize = sizeof(int);
	if((shmid = shmget(IPC_PRIVATE,allocationSize,IPC_CREAT|0600))<0) {
		ERROR("problems shmget'ing\n");
		exit(EXIT_FAILURE);
	}
	if((player_pid = shmat(shmid,NULL,0))<0) {
		ERROR("problems shmat'ing\n");
		exit(EXIT_FAILURE);
	}
	if (shmctl(shmid, IPC_RMID, 0)<0) {
		ERROR("problems shmctl'ing\n");
		exit(EXIT_FAILURE);
	}

	buffer = &(playerData_pd->buffer);

	buffer->chunks = ((char *)playerData_pd)+sizeof(PlayerData);
	buffer->chunkSize = (mpd_uint16 *)(((char *)buffer->chunks)+
				buffered_chunks*CHUNK_SIZE);
	buffer->bitRate = (mpd_uint16 *)(((char *)buffer->chunkSize)+
				buffered_chunks*sizeof(mpd_sint16));
	buffer->metaChunk = (mpd_sint8 *)(((char *)buffer->bitRate)+
				buffered_chunks*sizeof(mpd_sint16));
	buffer->times = (float *)(((char *)buffer->metaChunk)+
				buffered_chunks*sizeof(mpd_sint8));

	playerData_pd->playerControl.stop = 0;
	playerData_pd->playerControl.pause = 0;
	playerData_pd->playerControl.play = 0;
	playerData_pd->playerControl.error = PLAYER_ERROR_NOERROR;
	playerData_pd->playerControl.lockQueue = 0;
	playerData_pd->playerControl.unlockQueue = 0;
	playerData_pd->playerControl.state = PLAYER_STATE_STOP;
	playerData_pd->playerControl.queueState = PLAYER_QUEUE_BLANK;
	playerData_pd->playerControl.queueLockState = PLAYER_QUEUE_UNLOCKED;
	playerData_pd->playerControl.seek = 0;
	memset(playerData_pd->playerControl.utf8url, 0, MAXPATHLEN+1);
	memset(playerData_pd->playerControl.erroredUrl, 0, MAXPATHLEN+1);
	memset(playerData_pd->playerControl.currentUrl, 0, MAXPATHLEN+1);
	playerData_pd->playerControl.crossFade = crossfade;
	playerData_pd->playerControl.softwareVolume = 1000;
	playerData_pd->playerControl.totalPlayTime = 0;
	playerData_pd->playerControl.decode_pid = 0;
	playerData_pd->playerControl.metadataState = 
                        PLAYER_METADATA_STATE_WRITE;

	playerData_pd->decoderControl.stop = 0;
	playerData_pd->decoderControl.start = 0;
	playerData_pd->decoderControl.state = DECODE_STATE_STOP;
	playerData_pd->decoderControl.seek = 0;
	playerData_pd->decoderControl.error = DECODE_ERROR_NOERROR;
	memset(playerData_pd->decoderControl.utf8url, 0, MAXPATHLEN+1);
}

PlayerData * getPlayerData() {
	return playerData_pd;
}

int getPlayerPid() {
	return *player_pid;
}

void setPlayerPid(int pid) {
	*player_pid = pid;
}

void freePlayerData() {
	shmdt(playerData_pd);
	shmdt(player_pid);
}
