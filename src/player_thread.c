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

#include "player_thread.h"
#include "playerData.h"
#include "audio.h"
#include "pcm_utils.h"
#include "path.h"
#include "log.h"
#include "main_notify.h"
#include "crossfade.h"

enum xfade_state {
	XFADE_DISABLED = -1,
	XFADE_UNKNOWN = 0,
	XFADE_ENABLED = 1
};

static void quitDecode(void)
{
	dc_stop(&pc.notify);
	pc.state = PLAYER_STATE_STOP;
	pc.command = PLAYER_COMMAND_NONE;
	wakeup_main_task();
}

static int waitOnDecode(int *decodeWaitedOn)
{
	dc_command_wait(&pc.notify);

	if (dc.error != DECODE_ERROR_NOERROR) {
		assert(dc.next_song == NULL || dc.next_song->url != NULL);
		pc.errored_song = dc.next_song;
		pc.error = PLAYER_ERROR_FILE;
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
	double where;

	if (dc.state == DECODE_STATE_STOP ||
	    dc.error != DECODE_ERROR_NOERROR ||
	    dc.current_song != pc.next_song) {
		dc_stop(&pc.notify);
		*next = -1;
		ob_clear();
		dc_start_async(pc.next_song);
		waitOnDecode(decodeWaitedOn);
	}

	where = pc.seekWhere;
	if (where > pc.totalTime)
		where = pc.totalTime - 0.1;
	if (where < 0.0)
		where = 0.0;

	ret = dc_seek(&pc.notify, where);
	if (ret == 0)
		pc.elapsedTime = where;

	player_command_finished();

	return ret;
}

static void processDecodeInput(int *pause_r, unsigned int *bbp_r,
			       enum xfade_state *do_xfade_r,
			       int *decodeWaitedOn_r,
			       int *next_r)
{
	switch (pc.command) {
	case PLAYER_COMMAND_NONE:
	case PLAYER_COMMAND_PLAY:
	case PLAYER_COMMAND_STOP:
	case PLAYER_COMMAND_EXIT:
	case PLAYER_COMMAND_CLOSE_AUDIO:
		break;

	case PLAYER_COMMAND_LOCK_QUEUE:
		pc.queueLockState = PLAYER_QUEUE_LOCKED;
		player_command_finished();
		break;

	case PLAYER_COMMAND_UNLOCK_QUEUE:
		pc.queueLockState = PLAYER_QUEUE_UNLOCKED;
		player_command_finished();
		break;

	case PLAYER_COMMAND_PAUSE:
		*pause_r = !*pause_r;
		if (*pause_r) {
			pc.state = PLAYER_STATE_PAUSE;
		} else {
			if (openAudioDevice(NULL) >= 0) {
				pc.state = PLAYER_STATE_PLAY;
			} else {
				char tmp[MPD_PATH_MAX];
				assert(dc.next_song == NULL || dc.next_song->url != NULL);
				pc.errored_song = dc.next_song;
				pc.error = PLAYER_ERROR_AUDIO;
				ERROR("problems opening audio device "
				      "while playing \"%s\"\n",
				      get_song_url(tmp, dc.next_song));
				*pause_r = -1;
			}
		}
		player_command_finished();
		if (*pause_r == -1) {
			*pause_r = 1;
		} else if (*pause_r) {
			dropBufferedAudio();
			closeAudioDevice();
		}
		break;

	case PLAYER_COMMAND_SEEK:
		dropBufferedAudio();
		if (decodeSeek(decodeWaitedOn_r, next_r) == 0) {
			*do_xfade_r = XFADE_UNKNOWN;
			*bbp_r = 0;
		}
		break;
	}
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

	if (waitOnDecode(&decodeWaitedOn) < 0) {
		quitDecode();
		return;
	}

	pc.elapsedTime = 0;
	pc.state = PLAYER_STATE_PLAY;
	player_command_finished();

	while (1) {
		processDecodeInput(&do_pause, &bbp, &do_xfade,
				   &decodeWaitedOn, &next);
		if (pc.command == PLAYER_COMMAND_STOP ||
		    pc.command == PLAYER_COMMAND_EXIT ||
		    pc.command == PLAYER_COMMAND_CLOSE_AUDIO) {
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
					assert(dc.next_song == NULL || dc.next_song->url != NULL);
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
				assert(dc.next_song == NULL || dc.next_song->url != NULL);
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
			dc_start_async(pc.next_song);
			pc.queueState = PLAYER_QUEUE_DECODE;
			wakeup_main_task();
		}
		if (next >= 0 && do_xfade == XFADE_UNKNOWN &&
		    dc.command != DECODE_COMMAND_START &&
		    dc.state != DECODE_STATE_START) {
			/* enable cross fading in this song?  if yes,
			   calculate how many chunks will be required
			   for it */
			crossFadeChunks =
				cross_fade_calc(pc.crossFade, dc.totalTime,
						&(ob.audioFormat),
						ob.size -
						buffered_before_play);
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
					cross_fade_apply(beginChunk,
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

			/* wait for a signal from the playlist */
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
static void decode(void)
{
	ob_clear();

	dc_start(&pc.notify, pc.next_song);
	decodeParent();
}

static void * player_task(mpd_unused void *arg)
{
	notify_enter(&pc.notify);

	while (1) {
		switch (pc.command) {
		case PLAYER_COMMAND_PLAY:
			decode();
			break;

		case PLAYER_COMMAND_STOP:
		case PLAYER_COMMAND_SEEK:
		case PLAYER_COMMAND_PAUSE:
			player_command_finished();
			break;

		case PLAYER_COMMAND_CLOSE_AUDIO:
			closeAudioDevice();
			player_command_finished();
			break;

		case PLAYER_COMMAND_EXIT:
			closeAudioDevice();
			player_command_finished();
			pthread_exit(NULL);
			break;

		case PLAYER_COMMAND_LOCK_QUEUE:
			pc.queueLockState = PLAYER_QUEUE_LOCKED;
			player_command_finished();
			break;

		case PLAYER_COMMAND_UNLOCK_QUEUE:
			pc.queueLockState = PLAYER_QUEUE_UNLOCKED;
			player_command_finished();
			break;

		case PLAYER_COMMAND_NONE:
			notify_wait(&pc.notify);
			break;
		}
	}
	return NULL;
}

void player_create(void)
{
	pthread_attr_t attr;
	pthread_t player_thread;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&player_thread, &attr, player_task, NULL))
		FATAL("Failed to spawn player task: %s\n", strerror(errno));
}
