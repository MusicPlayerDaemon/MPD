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

#include "decode.h"

#include "player.h"
#include "playerData.h"
#include "utils.h"
#include "pcm_utils.h"
#include "audio.h"
#include "path.h"
#include "log.h"
#include "sig_handlers.h"
#include "ls.h"
#include "utf8.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

volatile int * volatile decode_pid = NULL;

void decodeSigHandler(int sig) {
	if(sig==SIGCHLD) {
		int status;
		if(decode_pid && *decode_pid==wait3(&status,WNOHANG,NULL)) {
			if(WIFSIGNALED(status) && WTERMSIG(status)!=SIGTERM &&
					WTERMSIG(status)!=SIGINT) 
			{
				ERROR("decode process died from signal: %i\n",
						WTERMSIG(status));
			}
			*decode_pid = 0;
		}
	}
	else if(sig==SIGTERM) {
		if(decode_pid) {
			int pid = *decode_pid;
			if(pid>0) kill(pid,SIGTERM);
		}
		exit(EXIT_SUCCESS);
	}
}

void stopDecode(DecoderControl * dc) {
	if(decode_pid && *decode_pid>0 && 
			(dc->start || dc->state!=DECODE_STATE_STOP)) 
	{
		dc->stop = 1;
		while(decode_pid && *decode_pid>0 && dc->stop) my_usleep(10000);
	}
}

void quitDecode(PlayerControl * pc, DecoderControl * dc) {
	stopDecode(dc);
        pc->metadataState = PLAYER_METADATA_STATE_READ;
	pc->state = PLAYER_STATE_STOP;
        dc->seek = 0;
	pc->play = 0;
	pc->stop = 0;
	pc->pause = 0;
	kill(getppid(),SIGUSR1);
}

int calculateCrossFadeChunks(PlayerControl * pc, AudioFormat * af) {
	long chunks;

	if(pc->crossFade<=0) return 0;

	chunks = (af->sampleRate*af->bits*af->channels/8.0/CHUNK_SIZE);
	chunks = (chunks*pc->crossFade+0.5);

	if(chunks>(buffered_chunks-buffered_before_play)) {
		chunks = buffered_chunks-buffered_before_play;
	}

	if(chunks<0) chunks = 0;

	return (int)chunks;
}

#define handleDecodeStart() \
        if(decodeWaitedOn) { \
                if(dc->state!=DECODE_STATE_START &&  *decode_pid > 0 && \
                                dc->error==DECODE_ERROR_NOERROR) \
                { \
                        decodeWaitedOn = 0; \
	                if(openAudioDevice(&(cb->audioFormat))<0) { \
		                strncpy(pc->erroredUrl, pc->utf8url, \
                                                MAXPATHLEN); \
		                pc->erroredUrl[MAXPATHLEN] = '\0'; \
		                pc->error = PLAYER_ERROR_AUDIO; \
		                quitDecode(pc,dc); \
		                return; \
	                } \
                        if(pc->metadataState == PLAYER_METADATA_STATE_WRITE && \
                                        dc->metadataSet) \
                        { \
                                memcpy(pc->metadata, dc->metadata, \
                                                DECODE_METADATA_LENGTH); \
                                pc->metadata[DECODE_METADATA_LENGTH-1] = '\0'; \
                                pc->title = dc->title; \
                                pc->artist = dc->artist; \
                                pc->album = dc->album; \
                        } \
                        pc->metadataState = PLAYER_METADATA_STATE_READ; \
	                pc->totalTime = dc->totalTime; \
	                pc->sampleRate = dc->audioFormat.sampleRate; \
	                pc->bits = dc->audioFormat.bits; \
	                pc->channels = dc->audioFormat.channels; \
                } \
                else if(dc->state!=DECODE_STATE_START || *decode_pid <= 0) { \
		        strncpy(pc->erroredUrl, pc->utf8url, MAXPATHLEN); \
		        pc->erroredUrl[MAXPATHLEN] = '\0'; \
		        pc->error = PLAYER_ERROR_FILE; \
		        quitDecode(pc,dc); \
		        return; \
                } \
                else { \
                        my_usleep(10000); \
                        continue; \
                } \
        }

int waitOnDecode(PlayerControl * pc, DecoderControl * dc, OutputBuffer * cb,
                int * decodeWaitedOn) 
{
        strncpy(pc->currentUrl, pc->utf8url, MAXPATHLEN);
        pc->currentUrl[MAXPATHLEN] = '\0';

	while(decode_pid && *decode_pid>0 && dc->start) my_usleep(10000);

	if(dc->start || dc->error!=DECODE_ERROR_NOERROR) {
		strncpy(pc->erroredUrl, pc->utf8url, MAXPATHLEN);
		pc->erroredUrl[MAXPATHLEN] = '\0';
		pc->error = PLAYER_ERROR_FILE;
		quitDecode(pc,dc);
		return -1;
	}

        pc->totalTime = pc->fileTime;
        pc->elapsedTime = 0;
        pc->bitRate = 0;
        pc->sampleRate = 0;
        pc->bits = 0;
        pc->channels = 0;
        *decodeWaitedOn = 1;

	return 0;
}

int decodeSeek(PlayerControl * pc, DecoderControl * dc, OutputBuffer * cb,
                int * decodeWaitedOn) 
{
        int ret = -1;

	if(decode_pid && *decode_pid>0) {
		cb->next = -1;
		if(dc->state==DECODE_STATE_STOP || dc->error || 
				strcmp(dc->utf8url, pc->utf8url)!=0) 
		{
			stopDecode(dc);
			cb->begin = 0;
			cb->end = 0;
			cb->wrap = 0;
			dc->error = 0;
			dc->start = 1;
			waitOnDecode(pc,dc,cb,decodeWaitedOn);
		}
		if(*decode_pid>0 && dc->state!=DECODE_STATE_STOP && 
                                dc->seekable) 
                {
			dc->seekWhere = pc->seekWhere > pc->totalTime-0.1 ?
						pc->totalTime-0.1 : 
						pc->seekWhere;
			dc->seekWhere = 0 > dc->seekWhere ? 0 : dc->seekWhere;
                        dc->seekError = 0;
			dc->seek = 1;
                        while(*decode_pid>0 && dc->seek) my_usleep(10000);
                        if(!dc->seekError) {
                                pc->elapsedTime = dc->seekWhere;
		                pc->beginTime = pc->elapsedTime;
                                ret = 0;
                        }
		}
	}
	pc->seek = 0;

        return ret;
}

#define processDecodeInput() \
        if(pc->cycleLogFiles) { \
                myfprintfCloseAndOpenLogFile(); \
                pc->cycleLogFiles = 0; \
        } \
	if(pc->lockQueue) { \
		pc->queueLockState = PLAYER_QUEUE_LOCKED; \
		pc->lockQueue = 0; \
	} \
	if(pc->unlockQueue) { \
		pc->queueLockState = PLAYER_QUEUE_UNLOCKED; \
		pc->unlockQueue = 0; \
	} \
	if(pc->pause) { \
		pause = !pause; \
		if(pause) pc->state = PLAYER_STATE_PAUSE; \
		else { \
			if(openAudioDevice(NULL)<0) { \
				strncpy(pc->erroredUrl, pc->utf8url, \
                                                MAXPATHLEN); \
				pc->erroredUrl[MAXPATHLEN] = '\0'; \
				pc->error = PLAYER_ERROR_AUDIO; \
				quitDecode(pc,dc); \
				return; \
			} \
			pc->state = PLAYER_STATE_PLAY; \
		} \
		pc->pause = 0; \
		kill(getppid(),SIGUSR1); \
		if(pause) closeAudioDevice(); \
	} \
	if(pc->seek) { \
		pc->totalPlayTime+= pc->elapsedTime-pc->beginTime; \
		if(decodeSeek(pc,dc,cb,&decodeWaitedOn) == 0) { \
		        doCrossFade = 0; \
		        nextChunk =  -1; \
                        bbp = 0; \
                } \
	} \
	if(pc->stop) { \
		pc->totalPlayTime+= pc->elapsedTime-pc->beginTime; \
		quitDecode(pc,dc); \
		return; \
	}

void decodeStart(PlayerControl * pc, OutputBuffer * cb, DecoderControl * dc) {
        int ret;
        InputStream inStream;
        InputPlugin * plugin;
        char * path;

        if(isRemoteUrl(pc->utf8url)) {
                path = utf8StrToLatin1Dup(pc->utf8url);
        }
	else path = strdup(rmp2amp(utf8ToFsCharset(pc->utf8url)));

	if(!path) {
		dc->error = DECODE_ERROR_FILE;
		dc->state = DECODE_STATE_STOP;
		dc->start = 0;
                return;
	}

        dc->metadataSet = 0;
        memset(dc->metadata, 0, DECODE_METADATA_LENGTH);
        dc->title = -1;
        dc->album = -1;
        dc->artist = -1;

        strncpy(dc->utf8url, pc->utf8url, MAXPATHLEN);
	dc->utf8url[MAXPATHLEN] = '\0';

        if(openInputStream(&inStream, path) < 0) {
		dc->error = DECODE_ERROR_FILE;
		dc->state = DECODE_STATE_STOP;
		dc->start = 0;
		free(path);
                return;
        }

        dc->seekable = inStream.seekable;
        dc->state = DECODE_STATE_START;
	dc->start = 0;

        while(!inputStreamAtEOF(&inStream) && bufferInputStream(&inStream) < 0
                        && !dc->stop);

        if(dc->stop) {
                dc->state = DECODE_STATE_STOP;
                dc->stop = 0;
		free(path);
                return;
        }

        if(inStream.metaTitle) {
                strncpy(dc->metadata, inStream.metaTitle, 
                                DECODE_METADATA_LENGTH-1);
                dc->title = 0;
                dc->metadataSet = 1;                
        }

        ret = DECODE_ERROR_UNKTYPE;
	if(isRemoteUrl(pc->utf8url)) {
                plugin = getInputPluginFromMimeType(inStream.mime);
                if(plugin == NULL) {
                        plugin = getInputPluginFromSuffix(
                                        getSuffix(dc->utf8url));
                }
                /* this is needed for bastard streams that don't have a suffix
                                or set the mimeType */
                if(plugin == NULL) {
                        plugin = getInputPluginFromName("mp3");
                }
                if(plugin && (plugin->streamTypes & INPUT_PLUGIN_STREAM_URL) &&
                                plugin->streamDecodeFunc) 
                {
                        ret = plugin->streamDecodeFunc(cb, dc, &inStream);
                }
	}
        else {
                plugin = getInputPluginFromSuffix(getSuffix(dc->utf8url));
                if(plugin && (plugin->streamTypes && INPUT_PLUGIN_STREAM_FILE))
                {
                        if(plugin->streamDecodeFunc) {
                                ret = plugin->streamDecodeFunc(cb, dc, 
                                                &inStream);
                        }
                        else if(plugin->fileDecodeFunc) {
                                closeInputStream(&inStream);
                                ret = plugin->fileDecodeFunc(cb, dc, path);
                        }
                }
        }

	if(ret<0 || ret == DECODE_ERROR_UNKTYPE) {
		strncpy(pc->erroredUrl, dc->utf8url, MAXPATHLEN);
		pc->erroredUrl[MAXPATHLEN] = '\0';
		if(ret != DECODE_ERROR_UNKTYPE) dc->error = DECODE_ERROR_FILE;
                else {
                        dc->error = DECODE_ERROR_UNKTYPE;
                        closeInputStream(&inStream);
                }
		dc->stop = 0;
		dc->state = DECODE_STATE_STOP;
	}

	free(path);
}

int decoderInit(PlayerControl * pc, OutputBuffer * cb, DecoderControl * dc) {
			
	int pid;
	decode_pid = &(pc->decode_pid);

	blockSignals();
	pid = fork();

	if(pid==0) {
		/* CHILD */
		unblockSignals();

		while(1) {
                        if(dc->cycleLogFiles) {
                                myfprintfCloseAndOpenLogFile();
                                dc->cycleLogFiles = 0;
                        }
			else if(dc->start || dc->seek) decodeStart(pc, cb, dc);
			else if(dc->stop) {
				dc->state = DECODE_STATE_STOP;
				dc->stop = 0;
			}
			else my_usleep(10000);
		}

		exit(EXIT_SUCCESS);
		/* END OF CHILD */
	}
	else if(pid<0) {
		unblockSignals();
		strncpy(pc->erroredUrl, pc->utf8url, MAXPATHLEN);
		pc->erroredUrl[MAXPATHLEN] = '\0';
		pc->error = PLAYER_ERROR_SYSTEM;
		return -1;
	}

	*decode_pid = pid;
	unblockSignals();

	return 0;
}

void decodeParent(PlayerControl * pc, DecoderControl * dc, OutputBuffer * cb) {
	int pause = 0;
	int quit = 0;
	int bbp = buffered_before_play;
	int doCrossFade = 0;
	int crossFadeChunks = 0;
	int fadePosition;
	int nextChunk = -1;
	int test;
        int decodeWaitedOn = 0;
	char silence[CHUNK_SIZE];

	memset(silence,0,CHUNK_SIZE);

	if(waitOnDecode(pc,dc,cb,&decodeWaitedOn)<0) return;

	pc->state = PLAYER_STATE_PLAY;
	pc->play = 0;
	pc->beginTime = pc->elapsedTime;
	kill(getppid(),SIGUSR1);

	while(*decode_pid>0 && !cb->wrap && cb->end-cb->begin<bbp && 
				dc->state!=DECODE_STATE_STOP) 
	{
		processDecodeInput();
		if(quit) return;
		my_usleep(10000);
	}

	while(!quit) {
		processDecodeInput();
                handleDecodeStart();
		if(dc->state==DECODE_STATE_STOP && 
			pc->queueState==PLAYER_QUEUE_FULL &&
			pc->queueLockState==PLAYER_QUEUE_UNLOCKED) 
		{
			cb->next = cb->end;
			dc->start = 1;
			pc->queueState = PLAYER_QUEUE_DECODE;
			kill(getppid(),SIGUSR1);
		}
		if(cb->next>=0 && doCrossFade==0 && !dc->start) {
			nextChunk = -1;
			if(isCurrentAudioFormat(&(cb->audioFormat))) {
				doCrossFade = 1;
				crossFadeChunks = 
				calculateCrossFadeChunks(pc,
                                        &(cb->audioFormat));
				if(!crossFadeChunks ||
						pc->crossFade>=dc->totalTime) 
				{
					doCrossFade = -1;
				}
			}
			else doCrossFade = -1;
		}
		if(pause) my_usleep(10000);
		else if((cb->begin!=cb->end || cb->wrap) && 
				cb->begin!=cb->next)
		{
			if(doCrossFade==1 && cb->next>=0 &&
					((cb->next>cb->begin && 
					(fadePosition=cb->next-cb->begin)
					<=crossFadeChunks) || 
					(cb->begin>cb->next &&
					(fadePosition=cb->next-cb->begin+
					buffered_chunks)<=crossFadeChunks)))
			{
				if(nextChunk<0) {
					crossFadeChunks = fadePosition;
				}
				test = cb->end;
				if(cb->wrap) test+=buffered_chunks;
				nextChunk = cb->begin+crossFadeChunks;
				if(nextChunk<test) {
					if(nextChunk>=buffered_chunks)
					{
						nextChunk -=  buffered_chunks;  
					}
					pcm_mix(cb->chunks+cb->begin*CHUNK_SIZE,
							cb->chunks+nextChunk*
							CHUNK_SIZE,
							cb->chunkSize[
								cb->begin],
							cb->chunkSize[
								nextChunk],
							&(cb->audioFormat),
							((float)fadePosition)/
							crossFadeChunks);
					if(cb->chunkSize[nextChunk]>
							cb->chunkSize[cb->begin]
							)
					{
						cb->chunkSize[cb->begin]
								= cb->chunkSize
								[nextChunk];
					}
				}
				else {
					if(dc->state==DECODE_STATE_STOP)
					{
						doCrossFade = -1;
					}
					else continue;
				}
			}
			pc->elapsedTime = cb->times[cb->begin];
			pc->bitRate = cb->bitRate[cb->begin];
			pcm_volumeChange(cb->chunks+cb->begin*
				CHUNK_SIZE,
				cb->chunkSize[cb->begin],
				&(cb->audioFormat),
				pc->softwareVolume);
			if(playAudio(cb->chunks+cb->begin*CHUNK_SIZE,
				cb->chunkSize[cb->begin])<0) 
			{
				quit = 1;
			}
			cb->begin++;
			if(cb->begin>=buffered_chunks) {
				cb->begin = 0;
				cb->wrap = 0;
			}
		}
		else if(cb->next==cb->begin) {
			pc->totalPlayTime+= pc->elapsedTime-
						pc->beginTime;
			if(doCrossFade==1 && nextChunk>=0) {
				nextChunk = cb->begin+crossFadeChunks;
				test = cb->end;
				if(cb->wrap) test+=buffered_chunks;
				if(nextChunk<test) {
					if(nextChunk>=buffered_chunks)
					{
						nextChunk -= buffered_chunks;
					}
					cb->begin = nextChunk;
				}	
			}
			while(pc->queueState==PLAYER_QUEUE_DECODE ||
					pc->queueLockState==PLAYER_QUEUE_LOCKED)
			{
				processDecodeInput();
				if(quit) {
					quitDecode(pc,dc);
					return;
				}
				my_usleep(10000);
			}
			if(pc->queueState!=PLAYER_QUEUE_PLAY) {
				quit = 1;
				break;
			}
			else {
				cb->next = -1;
				if(waitOnDecode(pc,dc,cb,&decodeWaitedOn)<0) {
                                        return;
                                }
				nextChunk = -1;
				doCrossFade = 0;
				crossFadeChunks = 0;
				pc->queueState = PLAYER_QUEUE_EMPTY;
				kill(getppid(),SIGUSR1);
			}
			pc->beginTime = cb->times[cb->begin];
		}
		else if(*decode_pid<=0 || 
				(dc->state==DECODE_STATE_STOP && !dc->start)) 
		{
			quit = 1;
			break;
		}
		else {
			if(playAudio(silence, CHUNK_SIZE) < 0) quit = 1;
		}
	}

	pc->totalPlayTime+= pc->elapsedTime-pc->beginTime; \
	quitDecode(pc,dc);
}

/* decode w/ buffering
 * this will fork another process
 * child process does decoding
 * parent process does playing audio
 */
void decode() {
	OutputBuffer * cb;
	PlayerControl * pc;
	DecoderControl * dc;

	cb = &(getPlayerData()->buffer);

	cb->begin = 0;
	cb->end = 0;
	cb->wrap = 0;
	pc = &(getPlayerData()->playerControl);
	dc = &(getPlayerData()->decoderControl);
	dc->error = 0;
	cb->next = -1;
        dc->seek = 0;
        dc->stop = 0;
	dc->start = 1;
        
	if(decode_pid==NULL || *decode_pid<=0) {
		if(decoderInit(pc,cb,dc)<0) return;
	}

        decodeParent(pc, dc, cb);
}

/* this is stuff for inputPlugins to use! */
#define copyStringToMetadata(string, element) { \
	if(string && (slen = strlen(string)) && \
                        pos < DECODE_METADATA_LENGTH-1) \
        { \
		strncpy(dc->metadata+pos, string, \
                                DECODE_METADATA_LENGTH-1-pos); \
		element = pos; \
		pos += slen+1; \
	} \
}

void copyMpdTagToDecoderControlMetadata(DecoderControl * dc, MpdTag * tag) {
	int pos = 0;
	int slen;

        if(dc->metadataSet) return;
        if(!tag) return;

	memset(dc->metadata, 0, DECODE_METADATA_LENGTH);
	
	copyStringToMetadata(tag->title, dc->title);
	copyStringToMetadata(tag->artist, dc->artist);
	copyStringToMetadata(tag->album, dc->album);

	dc->metadataSet = 1;
}
