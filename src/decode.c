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
#include "decoder_internal.h"

#include "player.h"
#include "playerData.h"
#include "pcm_utils.h"
#include "path.h"
#include "log.h"
#include "ls.h"
#include "main_notify.h"

enum xfade_state {
	XFADE_DISABLED = -1,
	XFADE_UNKNOWN = 0,
	XFADE_ENABLED = 1
};

static void dc_command_wait(void)
{
	while (dc.command != DECODE_COMMAND_NONE) {
		notify_signal(&dc.notify);
		notify_wait(&pc.notify);
	}
}

static void dc_command(enum decoder_command cmd)
{
	dc.command = cmd;
	dc_command_wait();
}

void dc_command_finished(void)
{
       assert(dc.command != DECODE_COMMAND_NONE);

       dc.command = DECODE_COMMAND_NONE;
       notify_signal(&pc.notify);
}

static void stopDecode(void)
{
	if (dc.command == DECODE_COMMAND_START ||
	    dc.state != DECODE_STATE_STOP)
		dc_command(DECODE_COMMAND_STOP);
}

static void quitDecode(void)
{
	stopDecode();
	pc.state = PLAYER_STATE_STOP;
	dc.command = DECODE_COMMAND_NONE;
	pc.play = 0;
	pc.stop = 0;
	pc.pause = 0;
	wakeup_main_task();
}

static unsigned calculateCrossFadeChunks(AudioFormat * af, float totalTime)
{
	unsigned int buffered_chunks, chunks;

	if (pc.crossFade == 0 || pc.crossFade >= totalTime ||
	    !isCurrentAudioFormat(af))
		return 0;

	assert(pc.crossFade > 0);
	assert(af->bits > 0);
	assert(af->channels > 0);
	assert(af->sampleRate > 0);

	chunks = (af->sampleRate * af->bits * af->channels / 8.0 / CHUNK_SIZE);
	chunks = (chunks * pc.crossFade + 0.5);

	buffered_chunks = ob.size;
	assert(buffered_chunks >= buffered_before_play);
	if (chunks > (buffered_chunks - buffered_before_play))
		chunks = buffered_chunks - buffered_before_play;

	return chunks;
}

static int waitOnDecode(int *decodeWaitedOn)
{
	while (dc.command == DECODE_COMMAND_START) {
		notify_signal(&dc.notify);
		notify_wait(&pc.notify);
	}

	if (dc.error != DECODE_ERROR_NOERROR) {
		pc.errored_song = dc.next_song;
		pc.error = PLAYER_ERROR_FILE;
		quitDecode();
		return -1;
	}

	pc.totalTime = pc.fileTime;
	pc.bitRate = 0;
	pc.sampleRate = 0;
	pc.bits = 0;
	pc.channels = 0;
	*decodeWaitedOn = 1;

	return 0;
}

static int decodeSeek(int *decodeWaitedOn, int *next)
{
	int ret = -1;

	if (dc.state == DECODE_STATE_STOP ||
	    dc.error != DECODE_ERROR_NOERROR ||
	    dc.current_song != pc.next_song) {
		stopDecode();
		*next = -1;
		ob_clear();
		dc.next_song = pc.next_song;
		dc.error = DECODE_ERROR_NOERROR;
		dc.command = DECODE_COMMAND_START;
		waitOnDecode(decodeWaitedOn);
	}
	if (dc.state != DECODE_STATE_STOP && dc.seekable) {
		*next = -1;
		dc.seekWhere = pc.seekWhere > pc.totalTime - 0.1 ?
		    pc.totalTime - 0.1 : pc.seekWhere;
		dc.seekWhere = 0 > dc.seekWhere ? 0 : dc.seekWhere;
		dc.seekError = 0;
		dc_command(DECODE_COMMAND_SEEK);
		if (!dc.seekError) {
			pc.elapsedTime = dc.seekWhere;
			ret = 0;
		}
	}
	pc.seek = 0;
	wakeup_main_task();

	return ret;
}

static void processDecodeInput(int *pause_r, unsigned int *bbp_r,
			       enum xfade_state *do_xfade_r,
			       int *decodeWaitedOn_r,
			       int *next_r)
{
	if(pc.lockQueue) {
		pc.queueLockState = PLAYER_QUEUE_LOCKED;
		pc.lockQueue = 0;
		wakeup_main_task();
	}
	if(pc.unlockQueue) {
		pc.queueLockState = PLAYER_QUEUE_UNLOCKED;
		pc.unlockQueue = 0;
		wakeup_main_task();
	}
	if(pc.pause) {
		*pause_r = !*pause_r;
		if (*pause_r) {
			pc.state = PLAYER_STATE_PAUSE;
		} else {
			if (openAudioDevice(NULL) >= 0) {
				pc.state = PLAYER_STATE_PLAY;
			} else {
				char tmp[MPD_PATH_MAX];
				pc.errored_song = dc.next_song;
				pc.error = PLAYER_ERROR_AUDIO;
				ERROR("problems opening audio device "
				      "while playing \"%s\"\n",
				      get_song_url(tmp, dc.next_song));
				*pause_r = -1;
			}
		}
		pc.pause = 0;
		wakeup_main_task();
		if (*pause_r == -1) {
			*pause_r = 1;
		} else if (*pause_r) {
			dropBufferedAudio();
			closeAudioDevice();
		}
	}
	if(pc.seek) {
		dropBufferedAudio();
		if (decodeSeek(decodeWaitedOn_r, next_r) == 0) {
			*do_xfade_r = XFADE_UNKNOWN;
			*bbp_r = 0;
		}
	}
}

static void decodeStart(void)
{
	struct decoder decoder;
	int ret;
	int close_instream = 1;
	InputStream inStream;
	InputPlugin *plugin = NULL;
	char path_max_fs[MPD_PATH_MAX];
	char path_max_utf8[MPD_PATH_MAX];

	if (!get_song_url(path_max_utf8, dc.next_song)) {
		dc.error = DECODE_ERROR_FILE;
		goto stop_no_close;
	}
	if (!isRemoteUrl(path_max_utf8)) {
		rmp2amp_r(path_max_fs,
		          utf8_to_fs_charset(path_max_fs, path_max_utf8));
	} else
		pathcpy_trunc(path_max_fs, path_max_utf8);

	dc.current_song = dc.next_song; /* NEED LOCK */
	if (openInputStream(&inStream, path_max_fs) < 0) {
		dc.error = DECODE_ERROR_FILE;
		goto stop_no_close;
	}

	dc.state = DECODE_STATE_START;
	dc.command = DECODE_COMMAND_NONE;

	/* for http streams, seekable is determined in bufferInputStream */
	dc.seekable = inStream.seekable;

	if (dc.command == DECODE_COMMAND_STOP)
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
			ret = plugin->streamDecodeFunc(&decoder, &inStream);
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
				decoder.plugin = plugin;
				ret = plugin->streamDecodeFunc(&decoder, &inStream);
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
				decoder.plugin = plugin;
				ret = plugin->streamDecodeFunc(&decoder,
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
				decoder.plugin = plugin;
				ret = plugin->fileDecodeFunc(&decoder,
				                             path_max_fs);
				break;
			} else if (plugin->streamDecodeFunc) {
				decoder.plugin = plugin;
				ret = plugin->streamDecodeFunc(&decoder, &inStream);
				break;
			}
		}
	}

	if (ret < 0 || ret == DECODE_ERROR_UNKTYPE) {
		if (ret != DECODE_ERROR_UNKTYPE)
			dc.error = DECODE_ERROR_FILE;
		else
			dc.error = DECODE_ERROR_UNKTYPE;
	}

stop:
	if (close_instream)
		closeInputStream(&inStream);
stop_no_close:
	dc.state = DECODE_STATE_STOP;
	dc.command = DECODE_COMMAND_NONE;
}

static void * decoder_task(mpd_unused void *arg)
{
	notify_enter(&dc.notify);

	while (1) {
		assert(dc.state == DECODE_STATE_STOP);

		if (dc.command == DECODE_COMMAND_START ||
		    dc.command == DECODE_COMMAND_SEEK) {
			decodeStart();
		} else if (dc.command == DECODE_COMMAND_STOP) {
			dc.command = DECODE_COMMAND_NONE;
			notify_signal(&pc.notify);
		} else {
			notify_wait(&dc.notify);
			notify_signal(&pc.notify);
		}
	}

	return NULL;
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

static void crossFade(ob_chunk * a, ob_chunk * b,
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

static int playChunk(ob_chunk * chunk,
		     const AudioFormat * format, double sizeToTime)
{
	pc.elapsedTime = chunk->times;
	pc.bitRate = chunk->bitRate;

	pcm_volumeChange(chunk->data, chunk->chunkSize,
			 format, pc.softwareVolume);

	if (playAudio(chunk->data,
		      chunk->chunkSize) < 0)
		return -1;

	pc.totalPlayTime += sizeToTime * chunk->chunkSize;
	return 0;
}

static void decodeParent(void)
{
	int do_pause = 0;
	int buffering = 1;
	unsigned int bbp = buffered_before_play;
	enum xfade_state do_xfade = XFADE_UNKNOWN;
	unsigned int crossFadeChunks = 0;
	/** the position of the next cross-faded chunk in the next
	    song */
	int nextChunk = 0;
	int decodeWaitedOn = 0;
	static const char silence[CHUNK_SIZE];
	double sizeToTime = 0.0;
	/** the position of the first chunk in the next song */
	int next = -1;

	ob_set_lazy(0);

	if (waitOnDecode(&decodeWaitedOn) < 0)
		return;

	pc.elapsedTime = 0;
	pc.state = PLAYER_STATE_PLAY;
	pc.play = 0;
	wakeup_main_task();

	while (1) {
		processDecodeInput(&do_pause, &bbp, &do_xfade,
				   &decodeWaitedOn, &next);
		if (pc.stop) {
			dropBufferedAudio();
			break;
		}

		if (buffering) {
			if (ob_available() < bbp) {
				/* not enough decoded buffer space yet */
				notify_wait(&pc.notify);
				continue;
			} else {
				/* buffering is complete */
				buffering = 0;
				ob_set_lazy(1);
			}
		}

		if (decodeWaitedOn) {
			if(dc.state!=DECODE_STATE_START &&
			   dc.error==DECODE_ERROR_NOERROR) {
				/* the decoder is ready and ok */
				decodeWaitedOn = 0;
				if(openAudioDevice(&(ob.audioFormat))<0) {
					char tmp[MPD_PATH_MAX];
					pc.errored_song = dc.next_song;
					pc.error = PLAYER_ERROR_AUDIO;
					ERROR("problems opening audio device "
					      "while playing \"%s\"\n",
					      get_song_url(tmp, dc.next_song));
					break;
				}

				if (do_pause) {
					dropBufferedAudio();
					closeAudioDevice();
				}
				pc.totalTime = dc.totalTime;
				pc.sampleRate = dc.audioFormat.sampleRate;
				pc.bits = dc.audioFormat.bits;
				pc.channels = dc.audioFormat.channels;
				sizeToTime = audioFormatSizeToTime(&ob.audioFormat);
			}
			else if(dc.state!=DECODE_STATE_START) {
				/* the decoder failed */
				pc.errored_song = dc.next_song;
				pc.error = PLAYER_ERROR_FILE;
				break;
			}
			else {
				/* the decoder is not yet ready; wait
				   some more */
				notify_wait(&pc.notify);
				continue;
			}
		}

		if (dc.state == DECODE_STATE_STOP &&
		    pc.queueState == PLAYER_QUEUE_FULL &&
		    pc.queueLockState == PLAYER_QUEUE_UNLOCKED) {
			/* the decoder has finished the current song;
			   make it decode the next song */
			next = ob.end;
			dc.next_song = pc.next_song;
			dc.error = DECODE_ERROR_NOERROR;
			dc.command = DECODE_COMMAND_START;
			pc.queueState = PLAYER_QUEUE_DECODE;
			wakeup_main_task();
			notify_signal(&dc.notify);
		}
		if (next >= 0 && do_xfade == XFADE_UNKNOWN &&
		    dc.command != DECODE_COMMAND_START &&
		    dc.state != DECODE_STATE_START) {
			/* enable cross fading in this song?  if yes,
			   calculate how many chunks will be required
			   for it */
			crossFadeChunks =
				calculateCrossFadeChunks(&(ob.audioFormat),
							 dc.totalTime);
			if (crossFadeChunks > 0) {
				do_xfade = XFADE_ENABLED;
				nextChunk = -1;
			} else
				/* cross fading is disabled or the
				   next song is too short */
				do_xfade = XFADE_DISABLED;
		}

		if (do_pause)
			notify_wait(&pc.notify);
		else if (!ob_is_empty() && (int)ob.begin != next) {
			ob_chunk *beginChunk = ob_get_chunk(ob.begin);
			unsigned int fadePosition;
			if (do_xfade == XFADE_ENABLED && next >= 0 &&
			    (fadePosition = ob_relative(next))
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
				nextChunk = ob_absolute(crossFadeChunks);
				if (nextChunk >= 0) {
					ob_set_lazy(1);
					crossFade(beginChunk,
						  ob_get_chunk(nextChunk),
						  &(ob.audioFormat),
						  fadePosition,
						  crossFadeChunks);
				} else {
					/* there are not enough
					   decoded chunks yet */
					if (dc.state == DECODE_STATE_STOP) {
						/* the decoder isn't
						   running, abort
						   cross fading */
						do_xfade = XFADE_DISABLED;
					} else {
						/* wait for the
						   decoder */
						ob_set_lazy(0);
						notify_wait(&pc.notify);
						continue;
					}
				}
			}

			/* play the current chunk */
			if (playChunk(beginChunk, &(ob.audioFormat),
				      sizeToTime) < 0)
				break;
			ob_shift();
			notify_signal(&dc.notify);
		} else if (!ob_is_empty() && (int)ob.begin == next) {
			/* at the beginning of a new song */

			if (do_xfade == XFADE_ENABLED && nextChunk >= 0) {
				/* the cross-fade is finished; skip
				   the section which was cross-faded
				   (and thus already played) */
				ob_skip(crossFadeChunks);
			}

			do_xfade = XFADE_UNKNOWN;

			/* wait for the decoder to work on the new song */
			if (pc.queueState == PLAYER_QUEUE_DECODE ||
			    pc.queueLockState == PLAYER_QUEUE_LOCKED) {
				notify_wait(&pc.notify);
				continue;
			}
			if (pc.queueState != PLAYER_QUEUE_PLAY)
				break;

			next = -1;
			if (waitOnDecode(&decodeWaitedOn) < 0)
				return;

			pc.queueState = PLAYER_QUEUE_EMPTY;
			wakeup_main_task();
		} else if (dc.state == DECODE_STATE_STOP &&
			   dc.command != DECODE_COMMAND_START) {
			break;
		} else {
			/*DEBUG("waiting for decoded audio, play silence\n");*/
			if (playAudio(silence, CHUNK_SIZE) < 0)
				break;
		}
	}

	quitDecode();
}

/* decode w/ buffering
 * this will fork another process
 * child process does decoding
 * parent process does playing audio
 */
void decode(void)
{
	ob_clear();
	dc.next_song = pc.next_song;
	dc.error = DECODE_ERROR_NOERROR;
	dc_command(DECODE_COMMAND_START);

	decodeParent();
}
