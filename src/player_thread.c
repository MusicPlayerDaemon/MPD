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
#include "player_control.h"
#include "decoder_control.h"
#include "audio.h"
#include "pcm_utils.h"
#include "path.h"
#include "log.h"
#include "main_notify.h"
#include "crossfade.h"
#include "song.h"

enum xfade_state {
	XFADE_DISABLED = -1,
	XFADE_UNKNOWN = 0,
	XFADE_ENABLED = 1
};

struct player {
	/**
	 * are we waiting for buffered_before_play?
	 */
	bool buffering;

	/**
	 * true if the decoder is starting and did not provide data
	 * yet
	 */
	bool decoder_starting;

	/**
	 * is the player paused?
	 */
	bool paused;

	/**
	 * is there a new song in pc.next_song?
	 */
	bool queued;

	/**
	 * is cross fading enabled?
	 */
	enum xfade_state xfade;

	/**
	 * index of the first chunk of the next song, -1 if there is
	 * no next song
	 */
	int next_song_chunk;
};

static void player_command_finished(void)
{
	assert(pc.command != PLAYER_COMMAND_NONE);

	pc.command = PLAYER_COMMAND_NONE;
	wakeup_main_task();
}

static void quitDecode(void)
{
	dc_stop(&pc.notify);
	pc.state = PLAYER_STATE_STOP;
	pc.command = PLAYER_COMMAND_NONE;
	wakeup_main_task();
}

static int waitOnDecode(struct player *player)
{
	dc_command_wait(&pc.notify);

	if (dc.error != DECODE_ERROR_NOERROR) {
		assert(dc.next_song == NULL || dc.next_song->url != NULL);
		pc.errored_song = dc.next_song;
		pc.error = PLAYER_ERROR_FILE;
		return -1;
	}

	pc.totalTime = pc.next_song->tag != NULL
		? pc.next_song->tag->time : 0;
	pc.bitRate = 0;
	audio_format_clear(&pc.audio_format);

	pc.next_song = NULL;
	player->queued = false;
	player->decoder_starting = true;

	return 0;
}

static int decodeSeek(struct player *player)
{
	int ret = -1;
	double where;

	if (decoder_current_song() != pc.next_song) {
		dc_stop(&pc.notify);
		player->next_song_chunk = -1;
		ob_clear();
		dc_start_async(pc.next_song);
		waitOnDecode(player);
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

static void processDecodeInput(struct player *player)
{
	switch (pc.command) {
	case PLAYER_COMMAND_NONE:
	case PLAYER_COMMAND_PLAY:
	case PLAYER_COMMAND_STOP:
	case PLAYER_COMMAND_EXIT:
	case PLAYER_COMMAND_CLOSE_AUDIO:
		break;

	case PLAYER_COMMAND_QUEUE:
		assert(pc.next_song != NULL);
		player->queued = true;
		player_command_finished();
		break;

	case PLAYER_COMMAND_PAUSE:
		player->paused = !player->paused;
		if (player->paused) {
			dropBufferedAudio();
			audio_output_pause_all();
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
				      song_get_url(dc.next_song, tmp));
				player->paused = true;
			}
		}
		player_command_finished();
		break;

	case PLAYER_COMMAND_SEEK:
		dropBufferedAudio();
		if (decodeSeek(player) == 0) {
			player->xfade = XFADE_UNKNOWN;

			/* abort buffering when the user has requested
			   a seek */
			player->buffering = false;
		}
		break;

	case PLAYER_COMMAND_CANCEL:
		if (pc.next_song == NULL) {
			/* the cancel request arrived too later, we're
			   already playing the queued song...  stop
			   everything now */
			pc.command = PLAYER_COMMAND_STOP;
			return;
		}

		if (player->next_song_chunk != -1) {
			/* the decoder is already decoding the song -
			   stop it and reset the position */
			dc_stop(&pc.notify);
			player->next_song_chunk = -1;
		}

		pc.next_song = NULL;
		player->queued = false;
		player_command_finished();
		break;
	}
}

static int playChunk(ob_chunk * chunk,
		     const struct audio_format *format, double sizeToTime)
{
	pc.elapsedTime = chunk->times;
	pc.bitRate = chunk->bitRate;

	pcm_volume(chunk->data, chunk->chunkSize,
		   format, pc.softwareVolume);

	if (playAudio(chunk->data,
		      chunk->chunkSize) < 0)
		return -1;

	pc.totalPlayTime += sizeToTime * chunk->chunkSize;
	return 0;
}

static void do_play(void)
{
	struct player player = {
		.buffering = true,
		.decoder_starting = false,
		.paused = false,
		.queued = false,
		.xfade = XFADE_UNKNOWN,
		.next_song_chunk = -1,
	};
	unsigned int crossFadeChunks = 0;
	/** the position of the next cross-faded chunk in the next
	    song */
	int nextChunk = 0;
	static const char silence[CHUNK_SIZE];
	double sizeToTime = 0.0;

	ob_clear();
	ob_set_lazy(false);

	dc_start(&pc.notify, pc.next_song);
	if (waitOnDecode(&player) < 0) {
		quitDecode();
		return;
	}

	pc.elapsedTime = 0;
	pc.state = PLAYER_STATE_PLAY;
	player_command_finished();

	while (1) {
		processDecodeInput(&player);
		if (pc.command == PLAYER_COMMAND_STOP ||
		    pc.command == PLAYER_COMMAND_EXIT ||
		    pc.command == PLAYER_COMMAND_CLOSE_AUDIO) {
			dropBufferedAudio();
			break;
		}

		if (player.buffering) {
			if (ob_available() < pc.buffered_before_play) {
				/* not enough decoded buffer space yet */
				notify_wait(&pc.notify);
				continue;
			} else {
				/* buffering is complete */
				player.buffering = false;
				ob_set_lazy(true);
			}
		}

		if (player.decoder_starting) {
			if (dc.error != DECODE_ERROR_NOERROR) {
				/* the decoder failed */
				assert(dc.next_song == NULL || dc.next_song->url != NULL);
				pc.errored_song = dc.next_song;
				pc.error = PLAYER_ERROR_FILE;
				break;
			}
			else if (!decoder_is_starting()) {
				/* the decoder is ready and ok */
				player.decoder_starting = false;
				if(openAudioDevice(&(ob.audioFormat))<0) {
					char tmp[MPD_PATH_MAX];
					assert(dc.next_song == NULL || dc.next_song->url != NULL);
					pc.errored_song = dc.next_song;
					pc.error = PLAYER_ERROR_AUDIO;
					ERROR("problems opening audio device "
					      "while playing \"%s\"\n",
					      song_get_url(dc.next_song, tmp));
					break;
				}

				if (player.paused) {
					dropBufferedAudio();
					closeAudioDevice();
				}
				pc.totalTime = dc.totalTime;
				pc.audio_format = dc.audioFormat;
				sizeToTime = audioFormatSizeToTime(&ob.audioFormat);
			}
			else {
				/* the decoder is not yet ready; wait
				   some more */
				notify_wait(&pc.notify);
				continue;
			}
		}

		if (decoder_is_idle() && !player.queued &&
		    pc.next_song != NULL) {
			/* the decoder has finished the current song;
			   request the next song from the playlist */
			pc.next_song = NULL;
			wakeup_main_task();
		}

		if (decoder_is_idle() && player.queued) {
			/* the decoder has finished the current song;
			   make it decode the next song */
			assert(pc.next_song != NULL);

			player.queued = false;
			player.next_song_chunk = ob.end;
			dc_start_async(pc.next_song);
		}
		if (player.next_song_chunk >= 0 &&
		    player.xfade == XFADE_UNKNOWN &&
		    !decoder_is_starting()) {
			/* enable cross fading in this song?  if yes,
			   calculate how many chunks will be required
			   for it */
			crossFadeChunks =
				cross_fade_calc(pc.crossFade, dc.totalTime,
						&(ob.audioFormat),
						ob.size -
						pc.buffered_before_play);
			if (crossFadeChunks > 0) {
				player.xfade = XFADE_ENABLED;
				nextChunk = -1;
			} else
				/* cross fading is disabled or the
				   next song is too short */
				player.xfade = XFADE_DISABLED;
		}

		if (player.paused)
			notify_wait(&pc.notify);
		else if (!ob_is_empty() &&
			 (int)ob.begin != player.next_song_chunk) {
			ob_chunk *beginChunk = ob_get_chunk(ob.begin);
			unsigned int fadePosition;
			if (player.xfade == XFADE_ENABLED &&
			    player.next_song_chunk >= 0 &&
			    (fadePosition = ob_relative(player.next_song_chunk))
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
					ob_set_lazy(true);
					cross_fade_apply(beginChunk,
							 ob_get_chunk(nextChunk),
							 &(ob.audioFormat),
							 fadePosition,
							 crossFadeChunks);
				} else {
					/* there are not enough
					   decoded chunks yet */
					if (decoder_is_idle()) {
						/* the decoder isn't
						   running, abort
						   cross fading */
						player.xfade = XFADE_DISABLED;
					} else {
						/* wait for the
						   decoder */
						ob_set_lazy(false);
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

			/* this formula should prevent that the
			   decoder gets woken up with each chunk; it
			   is more efficient to make it decode a
			   larger block at a time */
			if (ob_available() <= (pc.buffered_before_play + ob.size * 3) / 4)
				notify_signal(&dc.notify);
		} else if (!ob_is_empty() &&
			   (int)ob.begin == player.next_song_chunk) {
			/* at the beginning of a new song */

			if (player.xfade == XFADE_ENABLED && nextChunk >= 0) {
				/* the cross-fade is finished; skip
				   the section which was cross-faded
				   (and thus already played) */
				ob_skip(crossFadeChunks);
			}

			player.xfade = XFADE_UNKNOWN;

			player.next_song_chunk = -1;
			if (waitOnDecode(&player) < 0)
				return;

			wakeup_main_task();
		} else if (decoder_is_idle()) {
			break;
		} else {
			size_t frame_size =
				audio_format_frame_size(&pc.audio_format);
			/* this formula ensures that we don't send
			   partial frames */
			unsigned num_frames = CHUNK_SIZE / frame_size;

			/*DEBUG("waiting for decoded audio, play silence\n");*/
			if (playAudio(silence, num_frames * frame_size) < 0)
				break;
		}
	}

	quitDecode();
}

static void * player_task(mpd_unused void *arg)
{
	while (1) {
		switch (pc.command) {
		case PLAYER_COMMAND_PLAY:
		case PLAYER_COMMAND_QUEUE:
			do_play();
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

		case PLAYER_COMMAND_CANCEL:
			pc.next_song = NULL;
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
