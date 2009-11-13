/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "player_thread.h"
#include "player_control.h"
#include "decoder_control.h"
#include "decoder_thread.h"
#include "output_all.h"
#include "pcm_volume.h"
#include "path.h"
#include "event_pipe.h"
#include "crossfade.h"
#include "song.h"
#include "tag.h"
#include "pipe.h"
#include "chunk.h"
#include "idle.h"
#include "main.h"
#include "buffer.h"

#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "player_thread"

enum xfade_state {
	XFADE_DISABLED = -1,
	XFADE_UNKNOWN = 0,
	XFADE_ENABLED = 1
};

struct player {
	struct decoder_control *dc;

	struct music_pipe *pipe;

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
	 * the song currently being played
	 */
	struct song *song;

	/**
	 * is cross fading enabled?
	 */
	enum xfade_state xfade;

	/**
	 * has cross-fading begun?
	 */
	bool cross_fading;

	/**
	 * The number of chunks used for crossfading.
	 */
	unsigned cross_fade_chunks;

	/**
	 * The current audio format for the audio outputs.
	 */
	struct audio_format play_audio_format;

	/**
	 * The time stamp of the chunk most recently sent to the
	 * output thread.  This attribute is only used if
	 * audio_output_all_get_elapsed_time() didn't return a usable
	 * value; the output thread can estimate the elapsed time more
	 * precisly.
	 */
	float elapsed_time;
};

static struct music_buffer *player_buffer;

static void player_command_finished_locked(void)
{
	assert(pc.command != PLAYER_COMMAND_NONE);

	pc.command = PLAYER_COMMAND_NONE;
	g_cond_signal(main_cond);
}

static void player_command_finished(void)
{
	player_lock();
	player_command_finished_locked();
	player_unlock();
}

/**
 * Start the decoder.
 *
 * Player lock is not held.
 */
static void
player_dc_start(struct player *player, struct music_pipe *pipe)
{
	struct decoder_control *dc = player->dc;

	assert(player->queued || pc.command == PLAYER_COMMAND_SEEK);
	assert(pc.next_song != NULL);

	dc_start(dc, pc.next_song, player_buffer, pipe);
}

/**
 * Stop the decoder and clears (and frees) its music pipe.
 *
 * Player lock is not held.
 */
static void
player_dc_stop(struct player *player)
{
	struct decoder_control *dc = player->dc;

	dc_stop(dc);

	if (dc->pipe != NULL) {
		/* clear and free the decoder pipe */

		music_pipe_clear(dc->pipe, player_buffer);

		if (dc->pipe != player->pipe)
			music_pipe_free(dc->pipe);

		dc->pipe = NULL;
	}
}

/**
 * Returns true if the decoder is decoding the next song (or has begun
 * decoding it, or has finished doing it), and the player hasn't
 * switched to that song yet.
 */
static bool
decoding_next_song(const struct player *player)
{
	return player->dc->pipe != NULL && player->dc->pipe != player->pipe;
}

/**
 * After the decoder has been started asynchronously, wait for the
 * "START" command to finish.  The decoder may not be initialized yet,
 * i.e. there is no audio_format information yet.
 *
 * The player lock is not held.
 */
static bool
player_wait_for_decoder(struct player *player)
{
	struct decoder_control *dc = player->dc;

	assert(player->queued || pc.command == PLAYER_COMMAND_SEEK);
	assert(pc.next_song != NULL);

	player->queued = false;

	if (decoder_lock_has_failed(dc)) {
		player_lock();
		pc.errored_song = dc->song;
		pc.error = PLAYER_ERROR_FILE;
		pc.next_song = NULL;
		player_unlock();

		return false;
	}

	player->song = pc.next_song;
	player->elapsed_time = 0.0;

	/* set the "starting" flag, which will be cleared by
	   player_check_decoder_startup() */
	player->decoder_starting = true;

	player_lock();

	/* update player_control's song information */
	pc.total_time = pc.next_song->tag != NULL
		? pc.next_song->tag->time : 0;
	pc.bit_rate = 0;
	audio_format_clear(&pc.audio_format);

	/* clear the queued song */
	pc.next_song = NULL;

	player_unlock();

	/* call syncPlaylistWithQueue() in the main thread */
	event_pipe_emit(PIPE_EVENT_PLAYLIST);

	return true;
}

/**
 * The decoder has acknowledged the "START" command (see
 * player_wait_for_decoder()).  This function checks if the decoder
 * initialization has completed yet.
 *
 * The player lock is not held.
 */
static bool
player_check_decoder_startup(struct player *player)
{
	struct decoder_control *dc = player->dc;

	assert(player->decoder_starting);

	decoder_lock(dc);

	if (decoder_has_failed(dc)) {
		/* the decoder failed */
		decoder_unlock(dc);

		player_lock();
		pc.errored_song = dc->song;
		pc.error = PLAYER_ERROR_FILE;
		player_unlock();

		return false;
	} else if (!decoder_is_starting(dc)) {
		/* the decoder is ready and ok */

		decoder_unlock(dc);

		if (audio_format_defined(&player->play_audio_format) &&
		    !audio_output_all_wait(1))
			/* the output devices havn't finished playing
			   all chunks yet - wait for that */
			return true;

		player_lock();
		pc.total_time = dc->total_time;
		pc.audio_format = dc->in_audio_format;
		player_unlock();

		player->play_audio_format = dc->out_audio_format;
		player->decoder_starting = false;

		if (!player->paused &&
		    !audio_output_all_open(&dc->out_audio_format,
					   player_buffer)) {
			char *uri = song_get_uri(dc->song);
			g_warning("problems opening audio device "
				  "while playing \"%s\"", uri);
			g_free(uri);

			player_lock();
			pc.error = PLAYER_ERROR_AUDIO;

			/* pause: the user may resume playback as soon
			   as an audio output becomes available */
			pc.state = PLAYER_STATE_PAUSE;
			player_unlock();

			player->paused = true;
			return true;
		}

		return true;
	} else {
		/* the decoder is not yet ready; wait
		   some more */
		player_wait_decoder(dc);
		decoder_unlock(dc);

		return true;
	}
}

/**
 * Sends a chunk of silence to the audio outputs.  This is called when
 * there is not enough decoded data in the pipe yet, to prevent
 * underruns in the hardware buffers.
 *
 * The player lock is not held.
 */
static bool
player_send_silence(struct player *player)
{
	struct music_chunk *chunk;
	size_t frame_size =
		audio_format_frame_size(&player->play_audio_format);
	/* this formula ensures that we don't send
	   partial frames */
	unsigned num_frames = sizeof(chunk->data) / frame_size;

	assert(audio_format_defined(&player->play_audio_format));

	chunk = music_buffer_allocate(player_buffer);
	if (chunk == NULL) {
		g_warning("Failed to allocate silence buffer");
		return false;
	}

#ifndef NDEBUG
	chunk->audio_format = player->play_audio_format;
#endif

	chunk->times = -1.0; /* undefined time stamp */
	chunk->length = num_frames * frame_size;
	memset(chunk->data, 0, chunk->length);

	if (!audio_output_all_play(chunk)) {
		music_buffer_return(player_buffer, chunk);
		return false;
	}

	return true;
}

/**
 * This is the handler for the #PLAYER_COMMAND_SEEK command.
 *
 * The player lock is not held.
 */
static bool player_seek_decoder(struct player *player)
{
	struct decoder_control *dc = player->dc;
	double where;
	bool ret;

	assert(pc.next_song != NULL);

	if (decoder_current_song(dc) != pc.next_song) {
		/* the decoder is already decoding the "next" song -
		   stop it and start the previous song again */

		player_dc_stop(player);

		/* clear music chunks which might still reside in the
		   pipe */
		music_pipe_clear(player->pipe, player_buffer);

		/* re-start the decoder */
		player_dc_start(player, player->pipe);
		ret = player_wait_for_decoder(player);
		if (!ret) {
			/* decoder failure */
			player_command_finished();
			return false;
		}
	} else {
		pc.next_song = NULL;
		player->queued = false;
	}

	/* wait for the decoder to complete initialization */

	while (player->decoder_starting) {
		ret = player_check_decoder_startup(player);
		if (!ret) {
			/* decoder failure */
			player_command_finished();
			return false;
		}
	}

	/* send the SEEK command */

	where = pc.seek_where;
	if (where > pc.total_time)
		where = pc.total_time - 0.1;
	if (where < 0.0)
		where = 0.0;

	ret = dc_seek(dc, where);
	if (!ret) {
		/* decoder failure */
		player_command_finished();
		return false;
	}

	player->elapsed_time = where;

	player_command_finished();

	player->xfade = XFADE_UNKNOWN;

	/* re-fill the buffer after seeking */
	player->buffering = true;

	audio_output_all_cancel();

	return true;
}

/**
 * Player lock must be held before calling.
 */
static void player_process_command(struct player *player)
{
	struct decoder_control *dc = player->dc;

	switch (pc.command) {
	case PLAYER_COMMAND_NONE:
	case PLAYER_COMMAND_STOP:
	case PLAYER_COMMAND_EXIT:
	case PLAYER_COMMAND_CLOSE_AUDIO:
		break;

	case PLAYER_COMMAND_UPDATE_AUDIO:
		player_unlock();
		audio_output_all_enable_disable();
		player_lock();
		player_command_finished_locked();
		break;

	case PLAYER_COMMAND_QUEUE:
		assert(pc.next_song != NULL);
		assert(!player->queued);
		assert(dc->pipe == NULL || dc->pipe == player->pipe);

		player->queued = true;
		player_command_finished_locked();
		break;

	case PLAYER_COMMAND_PAUSE:
		player_unlock();

		player->paused = !player->paused;
		if (player->paused) {
			audio_output_all_pause();
			player_lock();

			pc.state = PLAYER_STATE_PAUSE;
		} else if (!audio_format_defined(&player->play_audio_format)) {
			/* the decoder hasn't provided an audio format
			   yet - don't open the audio device yet */
			player_lock();

			pc.state = PLAYER_STATE_PLAY;
		} else if (audio_output_all_open(&player->play_audio_format, player_buffer)) {
			/* unpaused, continue playing */
			player_lock();

			pc.state = PLAYER_STATE_PLAY;
		} else {
			/* the audio device has failed - rollback to
			   pause mode */
			pc.error = PLAYER_ERROR_AUDIO;

			player->paused = true;

			player_lock();
		}

		player_command_finished_locked();
		break;

	case PLAYER_COMMAND_SEEK:
		player_unlock();
		player_seek_decoder(player);
		player_lock();
		break;

	case PLAYER_COMMAND_CANCEL:
		if (pc.next_song == NULL) {
			/* the cancel request arrived too late, we're
			   already playing the queued song...  stop
			   everything now */
			pc.command = PLAYER_COMMAND_STOP;
			return;
		}

		if (decoding_next_song(player)) {
			/* the decoder is already decoding the song -
			   stop it and reset the position */
			player_unlock();
			player_dc_stop(player);
			player_lock();
		}

		pc.next_song = NULL;
		player->queued = false;
		player_command_finished_locked();
		break;

	case PLAYER_COMMAND_REFRESH:
		if (audio_format_defined(&player->play_audio_format) &&
		    !player->paused) {
			player_unlock();
			audio_output_all_check();
			player_lock();
		}

		pc.elapsed_time = audio_output_all_get_elapsed_time();
		if (pc.elapsed_time < 0.0)
			pc.elapsed_time = player->elapsed_time;

		player_command_finished_locked();
		break;
	}
}

static void
update_song_tag(struct song *song, const struct tag *new_tag)
{
	struct tag *old_tag;

	if (song_is_file(song))
		/* don't update tags of local files, only remote
		   streams may change tags dynamically */
		return;

	old_tag = song->tag;
	song->tag = tag_dup(new_tag);

	if (old_tag != NULL)
		tag_free(old_tag);

	/* the main thread will update the playlist version when he
	   receives this event */
	event_pipe_emit(PIPE_EVENT_TAG);

	/* notify all clients that the tag of the current song has
	   changed */
	idle_add(IDLE_PLAYER);
}

/**
 * Plays a #music_chunk object (after applying software volume).  If
 * it contains a (stream) tag, copy it to the current song, so MPD's
 * playlist reflects the new stream tag.
 *
 * Player lock is not held.
 */
static bool
play_chunk(struct song *song, struct music_chunk *chunk,
	   const struct audio_format *format)
{
	assert(music_chunk_check_format(chunk, format));

	if (chunk->tag != NULL)
		update_song_tag(song, chunk->tag);

	if (chunk->length == 0) {
		music_buffer_return(player_buffer, chunk);
		return true;
	}

	pc.bit_rate = chunk->bit_rate;

	/* send the chunk to the audio outputs */

	if (!audio_output_all_play(chunk))
		return false;

	pc.total_play_time += (double)chunk->length /
		audio_format_time_to_size(format);
	return true;
}

/**
 * Obtains the next chunk from the music pipe, optionally applies
 * cross-fading, and sends it to all audio outputs.
 *
 * @return true on success, false on error (playback will be stopped)
 */
static bool
play_next_chunk(struct player *player)
{
	struct decoder_control *dc = player->dc;
	struct music_chunk *chunk = NULL;
	unsigned cross_fade_position;
	bool success;

	if (!audio_output_all_wait(64))
		/* the output pipe is still large enough, don't send
		   another chunk */
		return true;

	if (player->xfade == XFADE_ENABLED &&
	    decoding_next_song(player) &&
	    (cross_fade_position = music_pipe_size(player->pipe))
	    <= player->cross_fade_chunks) {
		/* perform cross fade */
		struct music_chunk *other_chunk =
			music_pipe_shift(dc->pipe);

		if (!player->cross_fading) {
			/* beginning of the cross fade - adjust
			   crossFadeChunks which might be bigger than
			   the remaining number of chunks in the old
			   song */
			player->cross_fade_chunks = cross_fade_position;
			player->cross_fading = true;
		}

		if (other_chunk != NULL) {
			chunk = music_pipe_shift(player->pipe);
			assert(chunk != NULL);

			cross_fade_apply(chunk, other_chunk,
					 &dc->out_audio_format,
					 cross_fade_position,
					 player->cross_fade_chunks);
			music_buffer_return(player_buffer, other_chunk);
		} else {
			/* there are not enough decoded chunks yet */

			decoder_lock(dc);

			if (decoder_is_idle(dc)) {
				/* the decoder isn't running, abort
				   cross fading */
				decoder_unlock(dc);

				player->xfade = XFADE_DISABLED;
			} else {
				/* wait for the decoder */
				decoder_signal(dc);
				player_wait_decoder(dc);
				decoder_unlock(dc);

				return true;
			}
		}
	}

	if (chunk == NULL)
		chunk = music_pipe_shift(player->pipe);

	assert(chunk != NULL);

	/* play the current chunk */

	success = play_chunk(player->song, chunk, &player->play_audio_format);

	if (!success) {
		music_buffer_return(player_buffer, chunk);

		player_lock();

		pc.error = PLAYER_ERROR_AUDIO;

		/* pause: the user may resume playback as soon as an
		   audio output becomes available */
		pc.state = PLAYER_STATE_PAUSE;
		player->paused = true;

		player_unlock();

		return false;
	}

	/* this formula should prevent that the decoder gets woken up
	   with each chunk; it is more efficient to make it decode a
	   larger block at a time */
	decoder_lock(dc);
	if (!decoder_is_idle(dc) &&
	    music_pipe_size(dc->pipe) <= (pc.buffered_before_play +
					 music_buffer_size(player_buffer) * 3) / 4)
		decoder_signal(dc);
	decoder_unlock(dc);

	return true;
}

/**
 * This is called at the border between two songs: the audio output
 * has consumed all chunks of the current song, and we should start
 * sending chunks from the next one.
 *
 * The player lock is not held.
 *
 * @return true on success, false on error (playback will be stopped)
 */
static bool
player_song_border(struct player *player)
{
	char *uri;

	player->xfade = XFADE_UNKNOWN;

	uri = song_get_uri(player->song);
	g_message("played \"%s\"", uri);
	g_free(uri);

	music_pipe_free(player->pipe);
	player->pipe = player->dc->pipe;

	if (!player_wait_for_decoder(player))
		return false;

	return true;
}

/*
 * The main loop of the player thread, during playback.  This is
 * basically a state machine, which multiplexes data between the
 * decoder thread and the output threads.
 */
static void do_play(struct decoder_control *dc)
{
	struct player player = {
		.dc = dc,
		.buffering = true,
		.decoder_starting = false,
		.paused = false,
		.queued = true,
		.song = NULL,
		.xfade = XFADE_UNKNOWN,
		.cross_fading = false,
		.cross_fade_chunks = 0,
		.elapsed_time = 0.0,
	};

	player_unlock();

	player.pipe = music_pipe_new();

	player_dc_start(&player, player.pipe);
	if (!player_wait_for_decoder(&player)) {
		player_dc_stop(&player);
		player_command_finished();
		music_pipe_free(player.pipe);
		event_pipe_emit(PIPE_EVENT_PLAYLIST);
		player_lock();
		return;
	}

	player_lock();
	pc.state = PLAYER_STATE_PLAY;
	player_command_finished_locked();

	while (true) {
		player_process_command(&player);
		if (pc.command == PLAYER_COMMAND_STOP ||
		    pc.command == PLAYER_COMMAND_EXIT ||
		    pc.command == PLAYER_COMMAND_CLOSE_AUDIO) {
			player_unlock();
			audio_output_all_cancel();
			break;
		}

		player_unlock();

		if (player.buffering) {
			/* buffering at the start of the song - wait
			   until the buffer is large enough, to
			   prevent stuttering on slow machines */

			if (music_pipe_size(player.pipe) < pc.buffered_before_play &&
			    !decoder_lock_is_idle(dc)) {
				/* not enough decoded buffer space yet */

				if (!player.paused &&
				    audio_format_defined(&player.play_audio_format) &&
				    audio_output_all_check() < 4 &&
				    !player_send_silence(&player))
					break;

				decoder_lock(dc);
				/* XXX race condition: check decoder again */
				player_wait_decoder(dc);
				decoder_unlock(dc);
				player_lock();
				continue;
			} else {
				/* buffering is complete */
				player.buffering = false;
			}
		}

		if (player.decoder_starting) {
			/* wait until the decoder is initialized completely */
			bool success;

			success = player_check_decoder_startup(&player);
			if (!success)
				break;

			player_lock();
			continue;
		}

#ifndef NDEBUG
		/*
		music_pipe_check_format(&play_audio_format,
					player.next_song_chunk,
					&dc->out_audio_format);
		*/
#endif

		if (decoder_lock_is_idle(dc) && player.queued &&
		    dc->pipe == player.pipe) {
			/* the decoder has finished the current song;
			   make it decode the next song */
			assert(dc->pipe == NULL || dc->pipe == player.pipe);

			player_dc_start(&player, music_pipe_new());
		}

		if (decoding_next_song(&player) &&
		    player.xfade == XFADE_UNKNOWN &&
		    !decoder_lock_is_starting(dc)) {
			/* enable cross fading in this song?  if yes,
			   calculate how many chunks will be required
			   for it */
			player.cross_fade_chunks =
				cross_fade_calc(pc.cross_fade_seconds, dc->total_time,
						&dc->out_audio_format,
						&player.play_audio_format,
						music_buffer_size(player_buffer) -
						pc.buffered_before_play);
			if (player.cross_fade_chunks > 0) {
				player.xfade = XFADE_ENABLED;
				player.cross_fading = false;
			} else
				/* cross fading is disabled or the
				   next song is too short */
				player.xfade = XFADE_DISABLED;
		}

		if (player.paused) {
			player_lock();

			if (pc.command == PLAYER_COMMAND_NONE)
				player_wait();
			continue;
		} else if (music_pipe_size(player.pipe) > 0) {
			/* at least one music chunk is ready - send it
			   to the audio output */

			play_next_chunk(&player);
		} else if (audio_output_all_check() > 0) {
			/* not enough data from decoder, but the
			   output thread is still busy, so it's
			   okay */

			/* XXX synchronize in a better way */
			g_usleep(10000);
		} else if (decoding_next_song(&player)) {
			/* at the beginning of a new song */

			if (!player_song_border(&player))
				break;
		} else if (decoder_lock_is_idle(dc)) {
			/* check the size of the pipe again, because
			   the decoder thread may have added something
			   since we last checked */
			if (music_pipe_size(player.pipe) == 0) {
				/* wait for the hardware to finish
				   playback */
				audio_output_all_drain();
				break;
			}
		} else {
			/* the decoder is too busy and hasn't provided
			   new PCM data in time: send silence (if the
			   output pipe is empty) */
			if (!player_send_silence(&player))
				break;
		}

		player_lock();
	}

	player_dc_stop(&player);

	music_pipe_clear(player.pipe, player_buffer);
	music_pipe_free(player.pipe);

	player_lock();

	if (player.queued) {
		assert(pc.next_song != NULL);
		pc.next_song = NULL;
	}

	pc.state = PLAYER_STATE_STOP;

	player_unlock();

	event_pipe_emit(PIPE_EVENT_PLAYLIST);

	player_lock();
}

static gpointer player_task(G_GNUC_UNUSED gpointer arg)
{
	struct decoder_control dc;

	dc_init(&dc);
	decoder_thread_start(&dc);

	player_buffer = music_buffer_new(pc.buffer_chunks);

	player_lock();

	while (1) {
		switch (pc.command) {
		case PLAYER_COMMAND_QUEUE:
			assert(pc.next_song != NULL);

			do_play(&dc);
			break;

		case PLAYER_COMMAND_STOP:
			player_unlock();
			audio_output_all_cancel();
			player_lock();

			/* fall through */

		case PLAYER_COMMAND_SEEK:
		case PLAYER_COMMAND_PAUSE:
			pc.next_song = NULL;
			player_command_finished_locked();
			break;

		case PLAYER_COMMAND_CLOSE_AUDIO:
			player_unlock();

			audio_output_all_close();

			player_lock();
			player_command_finished_locked();

#ifndef NDEBUG
			/* in the DEBUG build, check for leaked
			   music_chunk objects by freeing the
			   music_buffer */
			music_buffer_free(player_buffer);
			player_buffer = music_buffer_new(pc.buffer_chunks);
#endif

			break;

		case PLAYER_COMMAND_UPDATE_AUDIO:
			player_unlock();
			audio_output_all_enable_disable();
			player_lock();
			player_command_finished_locked();
			break;

		case PLAYER_COMMAND_EXIT:
			player_unlock();

			dc_quit(&dc);
			dc_deinit(&dc);
			audio_output_all_close();
			music_buffer_free(player_buffer);

			player_command_finished();
			return NULL;

		case PLAYER_COMMAND_CANCEL:
			pc.next_song = NULL;
			player_command_finished_locked();
			break;

		case PLAYER_COMMAND_REFRESH:
			/* no-op when not playing */
			player_command_finished_locked();
			break;

		case PLAYER_COMMAND_NONE:
			player_wait();
			break;
		}
	}
}

void player_create(void)
{
	GError *e = NULL;

	assert(pc.thread == NULL);

	pc.thread = g_thread_create(player_task, NULL, true, &e);
	if (pc.thread == NULL)
		g_error("Failed to spawn player task: %s", e->message);
}
