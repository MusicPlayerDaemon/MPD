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
#include "os_compat.h"

/* called inside decoder_task (inputPlugins) */
void decoder_wakeup_player(void)
{
	wakeup_player_nb();
}

void decoder_sleep(void)
{
	DecoderControl *dc = &(getPlayerData()->decoderControl);
	notifyWait(&dc->notify);
	wakeup_player_nb();
}

static void player_wakeup_decoder_nb(void)
{
	DecoderControl *dc = &(getPlayerData()->decoderControl);
	notifySignal(&dc->notify);
}

/* called from player_task */
static void player_wakeup_decoder(void)
{
	DecoderControl *dc = &(getPlayerData()->decoderControl);
	notifySignal(&dc->notify);
	player_sleep();
}

static void stopDecode(DecoderControl * dc)
{
	if (dc->start || dc->state != DECODE_STATE_STOP) {
		dc->stop = 1;
		do { player_wakeup_decoder_nb(); } while (dc->stop);
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
	wakeup_main_task();
}

static unsigned calculateCrossFadeChunks(PlayerControl * pc, AudioFormat * af,
					 float totalTime)
{
	long chunks;

	if (pc->crossFade <= 0 || pc->crossFade >= totalTime ||
	    !isCurrentAudioFormat(af))
		return 0;

	chunks = (af->sampleRate * af->bits * af->channels / 8.0 / CHUNK_SIZE);
	chunks = (chunks * pc->crossFade + 0.5);

	assert(buffered_chunks >= buffered_before_play);
	if (chunks > (buffered_chunks - buffered_before_play)) {
		chunks = buffered_chunks - buffered_before_play;
	}

	if (chunks < 0)
		chunks = 0;

	return (unsigned)chunks;
}

static int waitOnDecode(PlayerControl * pc, DecoderControl * dc,
			OutputBuffer * cb, int *decodeWaitedOn)
{
	while (dc->start)
		player_wakeup_decoder();

	if (dc->error != DECODE_ERROR_NOERROR) {
		pc->errored_song = pc->current_song;
		pc->error = PLAYER_ERROR_FILE;
		quitDecode(pc, dc);
		return -1;
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

	if (dc->state == DECODE_STATE_STOP ||
	    dc->error != DECODE_ERROR_NOERROR ||
	    dc->current_song != pc->current_song) {
		stopDecode(dc);
		*next = -1;
		clearOutputBuffer(cb);
		dc->error = DECODE_ERROR_NOERROR;
		dc->start = 1;
		waitOnDecode(pc, dc, cb, decodeWaitedOn);
	}
	if (dc->state != DECODE_STATE_STOP && dc->seekable) {
		*next = -1;
		dc->seekWhere = pc->seekWhere > pc->totalTime - 0.1 ?
		    pc->totalTime - 0.1 : pc->seekWhere;
		dc->seekWhere = 0 > dc->seekWhere ? 0 : dc->seekWhere;
		dc->seekError = 0;
		dc->seek = 1;
		do { player_wakeup_decoder(); } while (dc->seek);
		if (!dc->seekError) {
			pc->elapsedTime = dc->seekWhere;
			ret = 0;
		}
	}
	pc->seek = 0;
	wakeup_main_task();

	return ret;
}

static void processDecodeInput(PlayerControl * pc, DecoderControl * dc,
			       OutputBuffer * cb,
			       int *pause_r, unsigned int *bbp_r,
			       int *doCrossFade_r,
			       int *decodeWaitedOn_r,
			       int *next_r)
{
	if(pc->lockQueue) {
		pc->queueLockState = PLAYER_QUEUE_LOCKED;
		pc->lockQueue = 0;
		wakeup_main_task();
	}
	if(pc->unlockQueue) {
		pc->queueLockState = PLAYER_QUEUE_UNLOCKED;
		pc->unlockQueue = 0;
		wakeup_main_task();
	}
	if(pc->pause) {
		*pause_r = !*pause_r;
		if (*pause_r) {
			pc->state = PLAYER_STATE_PAUSE;
		} else {
			if (openAudioDevice(NULL) >= 0) {
				pc->state = PLAYER_STATE_PLAY;
			} else {
				char tmp[MPD_PATH_MAX];
				pc->errored_song = pc->current_song;
				pc->error = PLAYER_ERROR_AUDIO;
				ERROR("problems opening audio device "
				      "while playing \"%s\"\n",
				      get_song_url(tmp, pc->current_song));
				*pause_r = -1;
			}
		}
		pc->pause = 0;
		wakeup_main_task();
		if (*pause_r == -1) {
			*pause_r = 1;
		} else if (*pause_r) {
			dropBufferedAudio();
			closeAudioDevice();
		}
	}
	if(pc->seek) {
		dropBufferedAudio();
		if(decodeSeek(pc,dc,cb,decodeWaitedOn_r,next_r) == 0) {
			*doCrossFade_r = 0;
			*bbp_r = 0;
		}
	}
}

static void decodeStart(PlayerControl * pc, OutputBuffer * cb,
			DecoderControl * dc)
{
	int ret;
	int close_instream = 1;
	InputStream inStream;
	InputPlugin *plugin = NULL;
	char path_max_fs[MPD_PATH_MAX];
	char path_max_utf8[MPD_PATH_MAX];

	if (!get_song_url(path_max_utf8, pc->current_song)) {
		dc->error = DECODE_ERROR_FILE;
		goto stop_no_close;
	}
	if (!isRemoteUrl(path_max_utf8)) {
		rmp2amp_r(path_max_fs,
		          utf8_to_fs_charset(path_max_fs, path_max_utf8));
	}

	dc->current_song = pc->current_song; /* NEED LOCK */
	if (openInputStream(&inStream, path_max_fs) < 0) {
		dc->error = DECODE_ERROR_FILE;
		goto stop_no_close;
	}

	dc->state = DECODE_STATE_START;
	dc->start = 0;

	/* for http streams, seekable is determined in bufferInputStream */
	dc->seekable = inStream.seekable;

	if (dc->stop)
		goto stop;

	ret = DECODE_ERROR_UNKTYPE;
	if (isRemoteUrl(path_max_utf8)) {
		unsigned int next = 0;

		/* first we try mime types: */
		while (ret && (plugin = getInputPluginFromMimeType(inStream.mime, next++))) {
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
			const char *s = getSuffix(path_max_utf8);
			next = 0;
			while (ret && (plugin = getInputPluginFromSuffix(s, next++))) {
				if (!plugin->streamDecodeFunc)
					continue;
				if (!(plugin->streamTypes &
				      INPUT_PLUGIN_STREAM_URL))
					continue;
				if (plugin->tryDecodeFunc &&
				    !plugin->tryDecodeFunc(&inStream))
					continue;
				ret = plugin->streamDecodeFunc(cb, dc, &inStream);
				break;
			}
		}
		/* fallback to mp3: */
		/* this is needed for bastard streams that don't have a suffix
		   or set the mimeType */
		if (plugin == NULL) {
			/* we already know our mp3Plugin supports streams, no
			 * need to check for stream{Types,DecodeFunc} */
			if ((plugin = getInputPluginFromName("mp3"))) {
				ret = plugin->streamDecodeFunc(cb, dc,
				                               &inStream);
			}
		}
	} else {
		unsigned int next = 0;
		const char *s = getSuffix(path_max_utf8);
		while (ret && (plugin = getInputPluginFromSuffix(s, next++))) {
			if (!plugin->streamTypes & INPUT_PLUGIN_STREAM_FILE)
				continue;

			if (plugin->tryDecodeFunc &&
			    !plugin->tryDecodeFunc(&inStream))
				continue;

			if (plugin->fileDecodeFunc) {
				closeInputStream(&inStream);
				close_instream = 0;
				ret = plugin->fileDecodeFunc(cb, dc,
				                             path_max_fs);
				break;
			} else if (plugin->streamDecodeFunc) {
				ret = plugin->streamDecodeFunc(cb, dc, &inStream);
				break;
			}
		}
	}

	if (ret < 0 || ret == DECODE_ERROR_UNKTYPE) {
		pc->errored_song = pc->current_song;
		if (ret != DECODE_ERROR_UNKTYPE)
			dc->error = DECODE_ERROR_FILE;
		else
			dc->error = DECODE_ERROR_UNKTYPE;
	}

stop:
	if (close_instream)
		closeInputStream(&inStream);
stop_no_close:
	dc->state = DECODE_STATE_STOP;
	dc->stop = 0;
}

static void * decoder_task(mpd_unused void *unused)
{
	OutputBuffer *cb = &(getPlayerData()->buffer);
	PlayerControl *pc = &(getPlayerData()->playerControl);
	DecoderControl *dc = &(getPlayerData()->decoderControl);

	notifyEnter(&dc->notify);

	while (1) {
		if (dc->start || dc->seek) {
			decodeStart(pc, cb, dc);
		} else if (dc->stop) {
			dc->state = DECODE_STATE_STOP;
			dc->stop = 0;
			decoder_wakeup_player();
		} else {
			decoder_sleep();
		}
	}
}

void decoderInit(void)
{
	pthread_attr_t attr;
	pthread_t decoder_thread;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&decoder_thread, &attr, decoder_task, NULL))
		FATAL("Failed to spawn decoder task: %s\n", strerror(errno));
}

static void advanceOutputBufferTo(OutputBuffer * cb, int to)
{
	cb->begin = to;
}

static void crossFade(OutputBufferChunk * a, OutputBufferChunk * b,
		      AudioFormat * format,
		      unsigned int fadePosition, unsigned int crossFadeChunks)
{
	assert(fadePosition <= crossFadeChunks);

	pcm_mix(a->data,
		b->data,
		a->chunkSize,
		b->chunkSize,
		format,
		((float)fadePosition) /
		crossFadeChunks);
	if (b->chunkSize > a->chunkSize)
		a->chunkSize = b->chunkSize;
}

static int playChunk(PlayerControl * pc, OutputBufferChunk * chunk,
		     AudioFormat * format, double sizeToTime)
{
	pc->elapsedTime = chunk->times;
	pc->bitRate = chunk->bitRate;

	pcm_volumeChange(chunk->data, chunk->chunkSize,
			 format, pc->softwareVolume);

	if (playAudio(chunk->data,
		      chunk->chunkSize) < 0)
		return -1;

	pc->totalPlayTime +=
		sizeToTime * chunk->chunkSize;
	return 0;
}

static void decodeParent(PlayerControl * pc, DecoderControl * dc, OutputBuffer * cb)
{
	int pause = 0;
	int buffering = 1;
	unsigned int bbp = buffered_before_play;
	/** cross fading enabled for the current song? 0=must check;
	    1=enabled; -1=disabled */
	int doCrossFade = 0;
	unsigned int crossFadeChunks;
	/** the position of the next cross-faded chunk in the next
	    song */
	int nextChunk;
	int decodeWaitedOn = 0;
	static const char silence[CHUNK_SIZE];
	double sizeToTime = 0.0;
	unsigned int end;
	/** the position of the first chunk in the next song */
	int next = -1;

	if (waitOnDecode(pc, dc, cb, &decodeWaitedOn) < 0)
		return;

	pc->elapsedTime = 0;
	pc->state = PLAYER_STATE_PLAY;
	pc->play = 0;
	wakeup_main_task();

	while (1) {
		processDecodeInput(pc, dc, cb,
				   &pause, &bbp, &doCrossFade,
				   &decodeWaitedOn, &next);
		if (pc->stop) {
			dropBufferedAudio();
			break;
		}

		if (buffering) {
			if (availableOutputBuffer(cb) < bbp) {
				/* not enough decoded buffer space yet */
				player_sleep();
				continue;
			} else
				/* buffering is complete */
				buffering = 0;
		}

		if (decodeWaitedOn) {
			if(dc->state!=DECODE_STATE_START &&
			   dc->error==DECODE_ERROR_NOERROR) {
				/* the decoder is ready and ok */
				decodeWaitedOn = 0;
				if(openAudioDevice(&(cb->audioFormat))<0) {
					char tmp[MPD_PATH_MAX];
					pc->errored_song = pc->current_song;
					pc->error = PLAYER_ERROR_AUDIO;
					ERROR("problems opening audio device "
					      "while playing \"%s\"\n",
					      get_song_url(tmp, pc->current_song));
					break;
				} else {
					player_wakeup_decoder();
				}
				if (pause) {
					dropBufferedAudio();
					closeAudioDevice();
				}
				pc->totalTime = dc->totalTime;
				pc->sampleRate = dc->audioFormat.sampleRate;
				pc->bits = dc->audioFormat.bits;
				pc->channels = dc->audioFormat.channels;
				sizeToTime = audioFormatSizeToTime(&cb->audioFormat);
			}
			else if(dc->state!=DECODE_STATE_START) {
				/* the decoder failed */
				pc->errored_song = pc->current_song;
				pc->error = PLAYER_ERROR_FILE;
				break;
			}
			else {
				/* the decoder is not yet ready; wait
				   some more */
				player_sleep();
				continue;
			}
		}

		if (dc->state == DECODE_STATE_STOP &&
		    pc->queueState == PLAYER_QUEUE_FULL &&
		    pc->queueLockState == PLAYER_QUEUE_UNLOCKED) {
			/* the decoder has finished the current song;
			   make it decode the next song */
			next = cb->end;
			dc->start = 1;
			pc->queueState = PLAYER_QUEUE_DECODE;
			wakeup_main_task();
			player_wakeup_decoder_nb();
		}
		if (next >= 0 && doCrossFade == 0 && !dc->start &&
		    dc->state != DECODE_STATE_START) {
			/* enable cross fading in this song?  if yes,
			   calculate how many chunks will be required
			   for it */
			crossFadeChunks =
				calculateCrossFadeChunks(pc,
							 &(cb->
							   audioFormat),
							 dc->totalTime);
			if (crossFadeChunks > 0) {
				doCrossFade = 1;
				nextChunk = -1;
			} else
				/* cross fading is disabled or the
				   next song is too short */
				doCrossFade = -1;
		}

		/* copy these to local variables to prevent any potential
		   race conditions and weirdness */
		end = cb->end;

		if (pause)
			player_sleep();
		else if (!outputBufferEmpty(cb) && cb->begin != next) {
			OutputBufferChunk *beginChunk =
				outputBufferGetChunk(cb, cb->begin);
			unsigned int fadePosition;
			if (doCrossFade == 1 && next >= 0 &&
			    (fadePosition = outputBufferRelative(cb, next))
			    <= crossFadeChunks) {
				/* perform cross fade */
				if (nextChunk < 0) {
					/* beginning of the cross fade
					   - adjust crossFadeChunks
					   which might be bigger than
					   the remaining number of
					   chunks in the old song */
					crossFadeChunks = fadePosition;
				}
				nextChunk = outputBufferAbsolute(cb, crossFadeChunks);
				if (nextChunk >= 0) {
					crossFade(beginChunk,
						  outputBufferGetChunk(cb, nextChunk),
						  &(cb->audioFormat),
						  fadePosition,
						  crossFadeChunks);
				} else {
					/* there are not enough
					   decoded chunks yet */
					if (dc->state == DECODE_STATE_STOP) {
						/* the decoder isn't
						   running, abort
						   cross fading */
						doCrossFade = -1;
					} else {
						/* wait for the
						   decoder */
						player_sleep();
						continue;
					}
				}
			}

			/* play the current chunk */
			if (playChunk(pc, beginChunk, &(cb->audioFormat),
				      sizeToTime) < 0)
				break;
			outputBufferShift(cb);
			player_wakeup_decoder_nb();
		} else if (!outputBufferEmpty(cb) && cb->begin == next) {
			/* at the beginning of a new song */

			if (doCrossFade == 1 && nextChunk >= 0) {
				/* the cross-fade is finished; skip
				   the section which was cross-faded
				   (and thus already played) */
				nextChunk = outputBufferAbsolute(cb, crossFadeChunks);
				if (nextChunk >= 0)
					advanceOutputBufferTo(cb, nextChunk);
			}

			doCrossFade = 0;

			/* wait for the decoder to work on the new song */
			if (pc->queueState == PLAYER_QUEUE_DECODE ||
			    pc->queueLockState == PLAYER_QUEUE_LOCKED) {
				player_sleep();
				continue;
			}
			if (pc->queueState != PLAYER_QUEUE_PLAY)
				break;

			next = -1;
			if (waitOnDecode(pc, dc, cb, &decodeWaitedOn) < 0)
				return;

			pc->queueState = PLAYER_QUEUE_EMPTY;
			wakeup_main_task();
		} else if (dc->state == DECODE_STATE_STOP && !dc->start) {
			break;
		} else {
			/*DEBUG("waiting for decoded audio, play silence\n");*/
			if (playAudio(silence, CHUNK_SIZE) < 0)
				break;
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
	clearOutputBuffer(cb);

	pc = &(getPlayerData()->playerControl);
	dc = &(getPlayerData()->decoderControl);
	dc->error = DECODE_ERROR_NOERROR;
	dc->seek = 0;
	dc->stop = 0;
	dc->start = 1;
	do { player_wakeup_decoder(); } while (dc->start);

	decodeParent(pc, dc, cb);
}
