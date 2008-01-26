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

static int decode_pid;

void decodeSigHandler(int sig, siginfo_t * si, void *v)
{
	if (sig == SIGCHLD) {
		int status;
		if (decode_pid == wait3(&status, WNOHANG, NULL)) {
			/*
			if (WIFSIGNALED(status)) {
				if (WTERMSIG(status) != SIGTERM) {
					ERROR("decode process died from "
					      "signal: %i\n", WTERMSIG(status));
				}
			}
			*/
			decode_pid = 0;
			getPlayerData()->playerControl.decode_pid = 0;
		}
	} else if (sig == SIGTERM) {
		int pid = decode_pid;
		if (pid > 0) {
			/* DEBUG("player (or child) got SIGTERM\n"); */
			kill(pid, SIGTERM);
		} /* else
			DEBUG("decoder (or child) got SIGTERM\n"); */
		exit(EXIT_SUCCESS);
	}
}

static void stopDecode(DecoderControl * dc)
{
	if (decode_pid > 0 && (dc->start || dc->state != DECODE_STATE_STOP)) {
		dc->stop = 1;
		while (decode_pid > 0 && dc->stop)
			my_usleep(10000);
	}
}

static void quitDecode(PlayerControl * pc, DecoderControl * dc)
{
	stopDecode(dc);
	pc->state = PLAYER_STATE_STOP;
	dc->seek = 0;
	pc->play = 0;
	pc->stop = 0;
	pc->pause = 0;
	kill(getppid(), SIGUSR1);
}

static int calculateCrossFadeChunks(PlayerControl * pc, AudioFormat * af)
{
	long chunks;

	if (pc->crossFade <= 0)
		return 0;

	chunks = (af->sampleRate * af->bits * af->channels / 8.0 / CHUNK_SIZE);
	chunks = (chunks * pc->crossFade + 0.5);

	if (chunks > (buffered_chunks - buffered_before_play)) {
		chunks = buffered_chunks - buffered_before_play;
	}

	if (chunks < 0)
		chunks = 0;

	return (int)chunks;
}

#define handleDecodeStart() \
	if(decodeWaitedOn) { \
		if(dc->state!=DECODE_STATE_START &&  decode_pid > 0 && \
				dc->error==DECODE_ERROR_NOERROR) \
		{ \
			decodeWaitedOn = 0; \
			if(openAudioDevice(&(cb->audioFormat))<0) { \
				pathcpy_trunc(pc->erroredUrl, pc->utf8url); \
				pc->error = PLAYER_ERROR_AUDIO; \
				ERROR("problems opening audio device while playing \"%s\"\n", pc->utf8url); \
				quitDecode(pc,dc); \
				return; \
	                } \
			pc->totalTime = dc->totalTime; \
			pc->sampleRate = dc->audioFormat.sampleRate; \
			pc->bits = dc->audioFormat.bits; \
			pc->channels = dc->audioFormat.channels; \
			sizeToTime = 8.0/cb->audioFormat.bits/ \
					cb->audioFormat.channels/ \
					cb->audioFormat.sampleRate; \
                } \
                else if(dc->state!=DECODE_STATE_START || decode_pid <= 0) { \
			pathcpy_trunc(pc->erroredUrl, pc->utf8url); \
		        pc->error = PLAYER_ERROR_FILE; \
		        quitDecode(pc,dc); \
		        return; \
                } \
                else { \
			my_usleep(10000); \
                        continue; \
		} \
	}

static int waitOnDecode(PlayerControl * pc, DecoderControl * dc,
			OutputBuffer * cb, int *decodeWaitedOn)
{
	MpdTag *tag = NULL;
	pathcpy_trunc(pc->currentUrl, pc->utf8url);

	while (decode_pid > 0 && dc->start)
		my_usleep(10000);

	if (dc->start || dc->error != DECODE_ERROR_NOERROR) {
		pathcpy_trunc(pc->erroredUrl, pc->utf8url);
		pc->error = PLAYER_ERROR_FILE;
		quitDecode(pc, dc);
		return -1;
	}

	if ((tag = metadataChunkToMpdTagDup(&(pc->fileMetadataChunk)))) {
		sendMetadataToAudioDevice(tag);
		freeMpdTag(tag);
	}

	pc->totalTime = pc->fileTime;
	pc->bitRate = 0;
	pc->sampleRate = 0;
	pc->bits = 0;
	pc->channels = 0;
	*decodeWaitedOn = 1;

	return 0;
}

static int decodeSeek(PlayerControl * pc, DecoderControl * dc,
		      OutputBuffer * cb, int *decodeWaitedOn, int *next)
{
	int ret = -1;

	if (decode_pid > 0) {
		if (dc->state == DECODE_STATE_STOP || dc->error ||
		    strcmp(dc->utf8url, pc->utf8url) != 0) {
			stopDecode(dc);
			*next = -1;
			cb->begin = 0;
			cb->end = 0;
			dc->error = 0;
			dc->start = 1;
			waitOnDecode(pc, dc, cb, decodeWaitedOn);
		}
		if (decode_pid > 0 && dc->state != DECODE_STATE_STOP &&
		    dc->seekable) {
			*next = -1;
			dc->seekWhere = pc->seekWhere > pc->totalTime - 0.1 ?
			    pc->totalTime - 0.1 : pc->seekWhere;
			dc->seekWhere = 0 > dc->seekWhere ? 0 : dc->seekWhere;
			dc->seekError = 0;
			dc->seek = 1;
			while (decode_pid > 0 && dc->seek)
				my_usleep(10000);
			if (!dc->seekError) {
				pc->elapsedTime = dc->seekWhere;
				ret = 0;
			}
		}
	}
	pc->seek = 0;

	return ret;
}

#define processDecodeInput() \
        if(pc->cycleLogFiles) { \
                cycle_log_files(); \
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
		if (pause) pc->state = PLAYER_STATE_PAUSE; \
		else { \
			if (openAudioDevice(NULL) >= 0) pc->state = PLAYER_STATE_PLAY; \
			else { \
				pathcpy_trunc(pc->erroredUrl, pc->utf8url); \
				pc->error = PLAYER_ERROR_AUDIO; \
				ERROR("problems opening audio device while playing \"%s\"\n", pc->utf8url); \
				pause = -1; \
			} \
		} \
		pc->pause = 0; \
		kill(getppid(), SIGUSR1); \
		if (pause == -1) pause = 1; \
		else if (pause) { \
			dropBufferedAudio(); \
			closeAudioDevice(); \
		} \
	} \
	if(pc->seek) { \
		dropBufferedAudio(); \
		if(decodeSeek(pc,dc,cb,&decodeWaitedOn,&next) == 0) { \
		        doCrossFade = 0; \
		        nextChunk =  -1; \
                        bbp = 0; \
                } \
	} \
	if(pc->stop) { \
		dropBufferedAudio(); \
		quitDecode(pc,dc); \
		return; \
	}

static void decodeStart(PlayerControl * pc, OutputBuffer * cb,
			DecoderControl * dc)
{
	int ret;
	InputStream inStream;
	InputPlugin *plugin = NULL;
	char *path;

	if (isRemoteUrl(pc->utf8url))
		path = utf8StrToLatin1Dup(pc->utf8url);
	else
		path = xstrdup(rmp2amp(utf8ToFsCharset(pc->utf8url)));

	if (!path) {
		dc->error = DECODE_ERROR_FILE;
		dc->state = DECODE_STATE_STOP;
		dc->start = 0;
		return;
	}

	copyMpdTagToOutputBuffer(cb, NULL);

	pathcpy_trunc(dc->utf8url, pc->utf8url);

	if (openInputStream(&inStream, path) < 0) {
		dc->error = DECODE_ERROR_FILE;
		dc->state = DECODE_STATE_STOP;
		dc->start = 0;
		free(path);
		return;
	}

	dc->state = DECODE_STATE_START;
	dc->start = 0;

	while (!inputStreamAtEOF(&inStream) && bufferInputStream(&inStream) < 0
	       && !dc->stop) {
		/* sleep so we don't consume 100% of the cpu */
		my_usleep(1000);
	}

	/* for http streams, seekable is determined in bufferInputStream */
	dc->seekable = inStream.seekable;
        
	if (dc->stop) {
		dc->state = DECODE_STATE_STOP;
		dc->stop = 0;
		free(path);
		return;
	}

	/*if(inStream.metaName) {
	   MpdTag * tag = newMpdTag();
	   tag->name = xstrdup(inStream.metaName);
	   copyMpdTagToOutputBuffer(cb, tag);
	   freeMpdTag(tag);
	   } */

	/* reset Metadata in OutputBuffer */

	ret = DECODE_ERROR_UNKTYPE;
	if (isRemoteUrl(dc->utf8url)) {
		unsigned int next = 0;
		cb->acceptMetadata = 1;

		/* first we try mime types: */
		while (ret
		       && (plugin =
			   getInputPluginFromMimeType(inStream.mime, next++))) {
			if (!plugin->streamDecodeFunc)
				continue;
			if (!(plugin->streamTypes & INPUT_PLUGIN_STREAM_URL))
				continue;
			if (plugin->tryDecodeFunc
			    && !plugin->tryDecodeFunc(&inStream))
				continue;
			ret = plugin->streamDecodeFunc(cb, dc, &inStream);
			break;
		}

		/* if that fails, try suffix matching the URL: */
		if (plugin == NULL) {
			char *s = getSuffix(dc->utf8url);
			next = 0;
			while (ret
			       && (plugin =
				   getInputPluginFromSuffix(s, next++))) {
				if (!plugin->streamDecodeFunc)
					continue;
				if (!(plugin->streamTypes &
				      INPUT_PLUGIN_STREAM_URL))
					continue;
				if (plugin->tryDecodeFunc &&
				    !plugin->tryDecodeFunc(&inStream))
					continue;
				ret =
				    plugin->streamDecodeFunc(cb, dc, &inStream);
				break;
			}
		}
		/* fallback to mp3: */
		/* this is needed for bastard streams that don't have a suffix
		   or set the mimeType */
		if (plugin == NULL) {
			/* we already know our mp3Plugin supports streams, no
			 * need to check for stream{Types,DecodeFunc} */
			if ((plugin = getInputPluginFromName("mp3")))
				ret = plugin->streamDecodeFunc(cb, dc,
							       &inStream);
		}
	} else {
		unsigned int next = 0;
		char *s = getSuffix(dc->utf8url);
		cb->acceptMetadata = 0;
		while (ret && (plugin = getInputPluginFromSuffix(s, next++))) {
			if (!plugin->streamTypes & INPUT_PLUGIN_STREAM_FILE)
				continue;
			if (plugin->tryDecodeFunc
			    && !plugin->tryDecodeFunc(&inStream))
				continue;

			if (plugin->streamDecodeFunc) {
				ret =
				    plugin->streamDecodeFunc(cb, dc, &inStream);
				break;
			} else if (plugin->fileDecodeFunc) {
				closeInputStream(&inStream);
				ret = plugin->fileDecodeFunc(cb, dc, path);
			}
		}
	}

	if (ret < 0 || ret == DECODE_ERROR_UNKTYPE) {
		pathcpy_trunc(pc->erroredUrl, dc->utf8url);
		if (ret != DECODE_ERROR_UNKTYPE)
			dc->error = DECODE_ERROR_FILE;
		else {
			dc->error = DECODE_ERROR_UNKTYPE;
			closeInputStream(&inStream);
		}
		dc->stop = 0;
		dc->state = DECODE_STATE_STOP;
	}

	free(path);
}

static int decoderInit(PlayerControl * pc, OutputBuffer * cb,
		       DecoderControl * dc)
{
	blockSignals();
	getPlayerData()->playerControl.decode_pid = 0;
	decode_pid = fork();

	if (decode_pid == 0) {
		/* CHILD */
		unblockSignals();

		while (1) {
			if (dc->cycleLogFiles) {
				cycle_log_files();
				dc->cycleLogFiles = 0;
			} else if (dc->start || dc->seek)
				decodeStart(pc, cb, dc);
			else if (dc->stop) {
				dc->state = DECODE_STATE_STOP;
				dc->stop = 0;
			} else
				my_usleep(10000);
		}

		exit(EXIT_SUCCESS);
		/* END OF CHILD */
	} else if (decode_pid < 0) {
		unblockSignals();
		pathcpy_trunc(pc->erroredUrl, pc->utf8url);
		pc->error = PLAYER_ERROR_SYSTEM;
		return -1;
	}
	DEBUG("decoder PID: %d\n", decode_pid);
	getPlayerData()->playerControl.decode_pid = decode_pid;
	unblockSignals();

	return 0;
}

static void handleMetadata(OutputBuffer * cb, PlayerControl * pc, int *previous,
			   int *currentChunkSent, MetadataChunk * currentChunk)
{
	if (cb->begin != cb->end) {
		int meta = cb->metaChunk[cb->begin];
		if (meta != *previous) {
			DEBUG("player: metadata change\n");
			if (meta >= 0 && cb->metaChunkSet[meta]) {
				DEBUG("player: new metadata from decoder!\n");
				memcpy(currentChunk,
				       cb->metadataChunks + meta,
				       sizeof(MetadataChunk));
				*currentChunkSent = 0;
				cb->metaChunkSet[meta] = 0;
			}
		}
		*previous = meta;
	}
	if (!(*currentChunkSent) && pc->metadataState ==
	    PLAYER_METADATA_STATE_WRITE) {
		MpdTag *tag = NULL;

		*currentChunkSent = 1;

		if ((tag = metadataChunkToMpdTagDup(currentChunk))) {
			sendMetadataToAudioDevice(tag);
			freeMpdTag(tag);
		}

		memcpy(&(pc->metadataChunk), currentChunk,
		       sizeof(MetadataChunk));
		pc->metadataState = PLAYER_METADATA_STATE_READ;
		kill(getppid(), SIGUSR1);
	}
}

static void advanceOutputBufferTo(OutputBuffer * cb, PlayerControl * pc,
				  int *previous, int *currentChunkSent,
				  MetadataChunk * currentChunk, int to)
{
	while (cb->begin != to) {
		handleMetadata(cb, pc, previous, currentChunkSent,
			       currentChunk);
		if (cb->begin + 1 >= buffered_chunks) {
			cb->begin = 0;
		}
		else cb->begin++;
	}
}

static void decodeParent(PlayerControl * pc, DecoderControl * dc, OutputBuffer * cb)
{
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
	double sizeToTime = 0.0;
	int previousMetadataChunk = -1;
	MetadataChunk currentMetadataChunk;
	int currentChunkSent = 1;
	int end;
	int next = -1;

	memset(silence, 0, CHUNK_SIZE);

	if (waitOnDecode(pc, dc, cb, &decodeWaitedOn) < 0)
		return;

	pc->elapsedTime = 0;
	pc->state = PLAYER_STATE_PLAY;
	pc->play = 0;
	kill(getppid(), SIGUSR1);

	while (decode_pid > 0 && 
	       cb->end - cb->begin < bbp &&
	       cb->end != buffered_chunks - 1 &&
	       dc->state != DECODE_STATE_STOP) {
		processDecodeInput();
		my_usleep(1000);
	}

	while (!quit) {
		processDecodeInput();
		handleDecodeStart();
		handleMetadata(cb, pc, &previousMetadataChunk,
			       &currentChunkSent, &currentMetadataChunk);
		if (dc->state == DECODE_STATE_STOP &&
		    pc->queueState == PLAYER_QUEUE_FULL &&
		    pc->queueLockState == PLAYER_QUEUE_UNLOCKED) {
			next = cb->end;
			dc->start = 1;
			pc->queueState = PLAYER_QUEUE_DECODE;
			kill(getppid(), SIGUSR1);
		}
		if (next >= 0 && doCrossFade == 0 && !dc->start &&
		    dc->state != DECODE_STATE_START) {
			nextChunk = -1;
			if (isCurrentAudioFormat(&(cb->audioFormat))) {
				doCrossFade = 1;
				crossFadeChunks =
				    calculateCrossFadeChunks(pc,
							     &(cb->
							       audioFormat));
				if (!crossFadeChunks
				    || pc->crossFade >= dc->totalTime) {
					doCrossFade = -1;
				}
			} else
				doCrossFade = -1;
		}

		/* copy these to local variables to prevent any potential
		   race conditions and weirdness */
		end = cb->end;

		if (pause)
			my_usleep(10000);
		else if (cb->begin != end && cb->begin != next) {
			if (doCrossFade == 1 && next >= 0 &&
			    ((next > cb->begin &&
			      (fadePosition = next - cb->begin)
			      <= crossFadeChunks) ||
			     (cb->begin > next &&
			      (fadePosition = next - cb->begin +
			       buffered_chunks) <= crossFadeChunks))) {
				if (nextChunk < 0) {
					crossFadeChunks = fadePosition;
				}
				test = end;
				if (end < cb->begin)
					test += buffered_chunks;
				nextChunk = cb->begin + crossFadeChunks;
				if (nextChunk < test) {
					if (nextChunk >= buffered_chunks) {
						nextChunk -= buffered_chunks;
					}
					pcm_mix(cb->chunks +
						cb->begin * CHUNK_SIZE,
						cb->chunks +
						nextChunk * CHUNK_SIZE,
						cb->chunkSize[cb->begin],
						cb->chunkSize[nextChunk],
						&(cb->audioFormat),
						((float)fadePosition) /
						crossFadeChunks);
					if (cb->chunkSize[nextChunk] >
					    cb->chunkSize[cb->begin]
					    ) {
						cb->chunkSize[cb->begin]
						    = cb->chunkSize[nextChunk];
					}
				} else {
					if (dc->state == DECODE_STATE_STOP) {
						doCrossFade = -1;
					} else
						continue;
				}
			}
			pc->elapsedTime = cb->times[cb->begin];
			pc->bitRate = cb->bitRate[cb->begin];
			pcm_volumeChange(cb->chunks + cb->begin *
					 CHUNK_SIZE,
					 cb->chunkSize[cb->begin],
					 &(cb->audioFormat),
					 pc->softwareVolume);
			if (playAudio(cb->chunks + cb->begin * CHUNK_SIZE,
				      cb->chunkSize[cb->begin]) < 0) {
				quit = 1;
			}
			pc->totalPlayTime +=
			    sizeToTime * cb->chunkSize[cb->begin];
			if (cb->begin + 1 >= buffered_chunks) {
				cb->begin = 0;
			} else
				cb->begin++;
		} else if (cb->begin != end && cb->begin == next) {
			if (doCrossFade == 1 && nextChunk >= 0) {
				nextChunk = cb->begin + crossFadeChunks;
				test = end;
				if (end < cb->begin)
					test += buffered_chunks;
				if (nextChunk < test) {
					if (nextChunk >= buffered_chunks) {
						nextChunk -= buffered_chunks;
					}
					advanceOutputBufferTo(cb, pc,
							      &previousMetadataChunk,
							      &currentChunkSent,
							      &currentMetadataChunk,
							      nextChunk);
				}
			}
			while (pc->queueState == PLAYER_QUEUE_DECODE ||
			       pc->queueLockState == PLAYER_QUEUE_LOCKED) {
				processDecodeInput();
				if (quit) {
					quitDecode(pc, dc);
					return;
				}
				my_usleep(10000);
			}
			if (pc->queueState != PLAYER_QUEUE_PLAY) {
				quit = 1;
				break;
			} else {
				next = -1;
				if (waitOnDecode(pc, dc, cb, &decodeWaitedOn) <
				    0) {
					return;
				}
				nextChunk = -1;
				doCrossFade = 0;
				crossFadeChunks = 0;
				pc->queueState = PLAYER_QUEUE_EMPTY;
				kill(getppid(), SIGUSR1);
			}
		} else if (decode_pid <= 0 ||
			   (dc->state == DECODE_STATE_STOP && !dc->start)) {
			quit = 1;
			break;
		} else {
			/*DEBUG("waiting for decoded audio, play silence\n");*/
			if (playAudio(silence, CHUNK_SIZE) < 0)
				quit = 1;
		}
	}

	quitDecode(pc, dc);
}

/* decode w/ buffering
 * this will fork another process
 * child process does decoding
 * parent process does playing audio
 */
void decode(void)
{
	OutputBuffer *cb;
	PlayerControl *pc;
	DecoderControl *dc;

	cb = &(getPlayerData()->buffer);

	clearAllMetaChunkSets(cb);
	cb->begin = 0;
	cb->end = 0;
	pc = &(getPlayerData()->playerControl);
	dc = &(getPlayerData()->decoderControl);
	dc->error = 0;
	dc->seek = 0;
	dc->stop = 0;
	dc->start = 1;

	if (decode_pid <= 0) {
		if (decoderInit(pc, cb, dc) < 0)
			return;
	}

	decodeParent(pc, dc, cb);
}
