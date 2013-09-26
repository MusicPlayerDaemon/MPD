/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "PlayerThread.hxx"
#include "DecoderThread.hxx"
#include "DecoderControl.hxx"
#include "MusicPipe.hxx"
#include "MusicBuffer.hxx"
#include "MusicChunk.hxx"
#include "Song.hxx"
#include "Main.hxx"
#include "system/FatalError.hxx"
#include "CrossFade.hxx"
#include "PlayerControl.hxx"
#include "OutputAll.hxx"
#include "tag/Tag.hxx"
#include "Idle.hxx"
#include "GlobalEvents.hxx"

#include <cmath>

#include <glib.h>

#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "player_thread"

enum xfade_state {
	XFADE_DISABLED = -1,
	XFADE_UNKNOWN = 0,
	XFADE_ENABLED = 1
};

struct player {
	struct player_control *pc;

	struct decoder_control *dc;

	MusicPipe *pipe;

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
	 * Was any audio output opened successfully?  It might have
	 * failed meanwhile, but was not explicitly closed by the
	 * player thread.  When this flag is unset, some output
	 * methods must not be called.
	 */
	bool output_open;

	/**
	 * the song currently being played
	 */
	Song *song;

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
	 * The tag of the "next" song during cross-fade.  It is
	 * postponed, and sent to the output thread when the new song
	 * really begins.
	 */
	Tag *cross_fade_tag;

	/**
	 * The current audio format for the audio outputs.
	 */
	AudioFormat play_audio_format;

	/**
	 * The time stamp of the chunk most recently sent to the
	 * output thread.  This attribute is only used if
	 * audio_output_all_get_elapsed_time() didn't return a usable
	 * value; the output thread can estimate the elapsed time more
	 * precisely.
	 */
	float elapsed_time;

	player(player_control *_pc, decoder_control *_dc)
		:pc(_pc), dc(_dc),
		 buffering(false),
		 decoder_starting(false),
		 paused(false),
		 queued(true),
		 output_open(false),
		 song(NULL),
		 xfade(XFADE_UNKNOWN),
		 cross_fading(false),
		 cross_fade_chunks(0),
		 cross_fade_tag(NULL),
		 elapsed_time(0.0) {}
};

static MusicBuffer *player_buffer;

static void
player_command_finished_locked(struct player_control *pc)
{
	assert(pc->command != PLAYER_COMMAND_NONE);

	pc->command = PLAYER_COMMAND_NONE;
	pc->ClientSignal();
}

static void
player_command_finished(struct player_control *pc)
{
	pc->Lock();
	player_command_finished_locked(pc);
	pc->Unlock();
}

/**
 * Start the decoder.
 *
 * Player lock is not held.
 */
static void
player_dc_start(struct player *player, MusicPipe &pipe)
{
	struct player_control *pc = player->pc;
	struct decoder_control *dc = player->dc;

	assert(player->queued || pc->command == PLAYER_COMMAND_SEEK);
	assert(pc->next_song != NULL);

	unsigned start_ms = pc->next_song->start_ms;
	if (pc->command == PLAYER_COMMAND_SEEK)
		start_ms += (unsigned)(pc->seek_where * 1000);

	dc->Start(pc->next_song->DupDetached(),
		  start_ms, pc->next_song->end_ms,
		  *player_buffer, pipe);
}

/**
 * Is the decoder still busy on the same song as the player?
 *
 * Note: this function does not check if the decoder is already
 * finished.
 */
static bool
player_dc_at_current_song(const struct player *player)
{
	assert(player != NULL);
	assert(player->pipe != NULL);

	return player->dc->pipe == player->pipe;
}

/**
 * Returns true if the decoder is decoding the next song (or has begun
 * decoding it, or has finished doing it), and the player hasn't
 * switched to that song yet.
 */
static bool
player_dc_at_next_song(const struct player *player)
{
	return player->dc->pipe != NULL && !player_dc_at_current_song(player);
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

	dc->Stop();

	if (dc->pipe != NULL) {
		/* clear and free the decoder pipe */

		dc->pipe->Clear(*player_buffer);

		if (dc->pipe != player->pipe)
			delete dc->pipe;

		dc->pipe = NULL;
	}
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
	struct player_control *pc = player->pc;
	struct decoder_control *dc = player->dc;

	assert(player->queued || pc->command == PLAYER_COMMAND_SEEK);
	assert(pc->next_song != NULL);

	player->queued = false;

	Error error = dc->LockGetError();
	if (error.IsDefined()) {
		pc->Lock();
		pc->SetError(PLAYER_ERROR_DECODER, std::move(error));

		pc->next_song->Free();
		pc->next_song = NULL;

		pc->Unlock();

		return false;
	}

	if (player->song != NULL)
		player->song->Free();

	player->song = pc->next_song;
	player->elapsed_time = 0.0;

	/* set the "starting" flag, which will be cleared by
	   player_check_decoder_startup() */
	player->decoder_starting = true;

	pc->Lock();

	/* update player_control's song information */
	pc->total_time = pc->next_song->GetDuration();
	pc->bit_rate = 0;
	pc->audio_format.Clear();

	/* clear the queued song */
	pc->next_song = NULL;

	pc->Unlock();

	/* call syncPlaylistWithQueue() in the main thread */
	GlobalEvents::Emit(GlobalEvents::PLAYLIST);

	return true;
}

/**
 * Returns the real duration of the song, comprising the duration
 * indicated by the decoder plugin.
 */
static double
real_song_duration(const Song *song, double decoder_duration)
{
	assert(song != NULL);

	if (decoder_duration <= 0.0)
		/* the decoder plugin didn't provide information; fall
		   back to Song::GetDuration() */
		return song->GetDuration();

	if (song->end_ms > 0 && song->end_ms / 1000.0 < decoder_duration)
		return (song->end_ms - song->start_ms) / 1000.0;

	return decoder_duration - song->start_ms / 1000.0;
}

/**
 * Wrapper for audio_output_all_open().  Upon failure, it pauses the
 * player.
 *
 * @return true on success
 */
static bool
player_open_output(struct player *player)
{
	struct player_control *pc = player->pc;

	assert(player->play_audio_format.IsDefined());
	assert(pc->state == PLAYER_STATE_PLAY ||
	       pc->state == PLAYER_STATE_PAUSE);

	Error error;
	if (audio_output_all_open(player->play_audio_format, *player_buffer,
				  error)) {
		player->output_open = true;
		player->paused = false;

		pc->Lock();
		pc->state = PLAYER_STATE_PLAY;
		pc->Unlock();

		idle_add(IDLE_PLAYER);

		return true;
	} else {
		g_warning("%s", error.GetMessage());

		player->output_open = false;

		/* pause: the user may resume playback as soon as an
		   audio output becomes available */
		player->paused = true;

		pc->Lock();
		pc->SetError(PLAYER_ERROR_OUTPUT, std::move(error));
		pc->state = PLAYER_STATE_PAUSE;
		pc->Unlock();

		idle_add(IDLE_PLAYER);

		return false;
	}
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
	struct player_control *pc = player->pc;
	struct decoder_control *dc = player->dc;

	assert(player->decoder_starting);

	dc->Lock();

	Error error = dc->GetError();
	if (error.IsDefined()) {
		/* the decoder failed */
		dc->Unlock();

		pc->Lock();
		pc->SetError(PLAYER_ERROR_DECODER, std::move(error));
		pc->Unlock();

		return false;
	} else if (!dc->IsStarting()) {
		/* the decoder is ready and ok */

		dc->Unlock();

		if (player->output_open &&
		    !audio_output_all_wait(pc, 1))
			/* the output devices havn't finished playing
			   all chunks yet - wait for that */
			return true;

		pc->Lock();
		pc->total_time = real_song_duration(dc->song, dc->total_time);
		pc->audio_format = dc->in_audio_format;
		pc->Unlock();

		idle_add(IDLE_PLAYER);

		player->play_audio_format = dc->out_audio_format;
		player->decoder_starting = false;

		if (!player->paused && !player_open_output(player)) {
			char *uri = dc->song->GetURI();
			g_warning("problems opening audio device "
				  "while playing \"%s\"", uri);
			g_free(uri);

			return true;
		}

		return true;
	} else {
		/* the decoder is not yet ready; wait
		   some more */
		dc->WaitForDecoder();
		dc->Unlock();

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
	assert(player->output_open);
	assert(player->play_audio_format.IsDefined());

	struct music_chunk *chunk = player_buffer->Allocate();
	if (chunk == NULL) {
		g_warning("Failed to allocate silence buffer");
		return false;
	}

#ifndef NDEBUG
	chunk->audio_format = player->play_audio_format;
#endif

	const size_t frame_size = player->play_audio_format.GetFrameSize();
	/* this formula ensures that we don't send
	   partial frames */
	unsigned num_frames = sizeof(chunk->data) / frame_size;

	chunk->times = -1.0; /* undefined time stamp */
	chunk->length = num_frames * frame_size;
	memset(chunk->data, 0, chunk->length);

	Error error;
	if (!audio_output_all_play(chunk, error)) {
		g_warning("%s", error.GetMessage());
		player_buffer->Return(chunk);
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
	struct player_control *pc = player->pc;
	Song *song = pc->next_song;
	struct decoder_control *dc = player->dc;

	assert(pc->next_song != NULL);

	const unsigned start_ms = song->start_ms;

	if (!dc->LockIsCurrentSong(song)) {
		/* the decoder is already decoding the "next" song -
		   stop it and start the previous song again */

		player_dc_stop(player);

		/* clear music chunks which might still reside in the
		   pipe */
		player->pipe->Clear(*player_buffer);

		/* re-start the decoder */
		player_dc_start(player, *player->pipe);
		if (!player_wait_for_decoder(player)) {
			/* decoder failure */
			player_command_finished(pc);
			return false;
		}
	} else {
		if (!player_dc_at_current_song(player)) {
			/* the decoder is already decoding the "next" song,
			   but it is the same song file; exchange the pipe */
			player->pipe->Clear(*player_buffer);
			delete player->pipe;
			player->pipe = dc->pipe;
		}

		pc->next_song->Free();
		pc->next_song = NULL;
		player->queued = false;
	}

	/* wait for the decoder to complete initialization */

	while (player->decoder_starting) {
		if (!player_check_decoder_startup(player)) {
			/* decoder failure */
			player_command_finished(pc);
			return false;
		}
	}

	/* send the SEEK command */

	double where = pc->seek_where;
	if (where > pc->total_time)
		where = pc->total_time - 0.1;
	if (where < 0.0)
		where = 0.0;

	if (!dc->Seek(where + start_ms / 1000.0)) {
		/* decoder failure */
		player_command_finished(pc);
		return false;
	}

	player->elapsed_time = where;

	player_command_finished(pc);

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
	struct player_control *pc = player->pc;
	gcc_unused struct decoder_control *dc = player->dc;

	switch (pc->command) {
	case PLAYER_COMMAND_NONE:
	case PLAYER_COMMAND_STOP:
	case PLAYER_COMMAND_EXIT:
	case PLAYER_COMMAND_CLOSE_AUDIO:
		break;

	case PLAYER_COMMAND_UPDATE_AUDIO:
		pc->Unlock();
		audio_output_all_enable_disable();
		pc->Lock();
		player_command_finished_locked(pc);
		break;

	case PLAYER_COMMAND_QUEUE:
		assert(pc->next_song != NULL);
		assert(!player->queued);
		assert(!player_dc_at_next_song(player));

		player->queued = true;
		player_command_finished_locked(pc);
		break;

	case PLAYER_COMMAND_PAUSE:
		pc->Unlock();

		player->paused = !player->paused;
		if (player->paused) {
			audio_output_all_pause();
			pc->Lock();

			pc->state = PLAYER_STATE_PAUSE;
		} else if (!player->play_audio_format.IsDefined()) {
			/* the decoder hasn't provided an audio format
			   yet - don't open the audio device yet */
			pc->Lock();

			pc->state = PLAYER_STATE_PLAY;
		} else {
			player_open_output(player);

			pc->Lock();
		}

		player_command_finished_locked(pc);
		break;

	case PLAYER_COMMAND_SEEK:
		pc->Unlock();
		player_seek_decoder(player);
		pc->Lock();
		break;

	case PLAYER_COMMAND_CANCEL:
		if (pc->next_song == NULL) {
			/* the cancel request arrived too late, we're
			   already playing the queued song...  stop
			   everything now */
			pc->command = PLAYER_COMMAND_STOP;
			return;
		}

		if (player_dc_at_next_song(player)) {
			/* the decoder is already decoding the song -
			   stop it and reset the position */
			pc->Unlock();
			player_dc_stop(player);
			pc->Lock();
		}

		pc->next_song->Free();
		pc->next_song = NULL;
		player->queued = false;
		player_command_finished_locked(pc);
		break;

	case PLAYER_COMMAND_REFRESH:
		if (player->output_open && !player->paused) {
			pc->Unlock();
			audio_output_all_check();
			pc->Lock();
		}

		pc->elapsed_time = audio_output_all_get_elapsed_time();
		if (pc->elapsed_time < 0.0)
			pc->elapsed_time = player->elapsed_time;

		player_command_finished_locked(pc);
		break;
	}
}

static void
update_song_tag(Song *song, const Tag &new_tag)
{
	if (song->IsFile())
		/* don't update tags of local files, only remote
		   streams may change tags dynamically */
		return;

	Tag *old_tag = song->tag;
	song->tag = new Tag(new_tag);

	delete old_tag;

	/* the main thread will update the playlist version when he
	   receives this event */
	GlobalEvents::Emit(GlobalEvents::TAG);

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
play_chunk(struct player_control *pc,
	   Song *song, struct music_chunk *chunk,
	   const AudioFormat format,
	   Error &error)
{
	assert(chunk->CheckFormat(format));

	if (chunk->tag != NULL)
		update_song_tag(song, *chunk->tag);

	if (chunk->length == 0) {
		player_buffer->Return(chunk);
		return true;
	}

	pc->Lock();
	pc->bit_rate = chunk->bit_rate;
	pc->Unlock();

	/* send the chunk to the audio outputs */

	if (!audio_output_all_play(chunk, error))
		return false;

	pc->total_play_time += (double)chunk->length /
		format.GetTimeToSize();
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
	struct player_control *pc = player->pc;
	struct decoder_control *dc = player->dc;

	if (!audio_output_all_wait(pc, 64))
		/* the output pipe is still large enough, don't send
		   another chunk */
		return true;

	unsigned cross_fade_position;
	struct music_chunk *chunk = NULL;
	if (player->xfade == XFADE_ENABLED &&
	    player_dc_at_next_song(player) &&
	    (cross_fade_position = player->pipe->GetSize())
	    <= player->cross_fade_chunks) {
		/* perform cross fade */
		music_chunk *other_chunk = dc->pipe->Shift();

		if (!player->cross_fading) {
			/* beginning of the cross fade - adjust
			   crossFadeChunks which might be bigger than
			   the remaining number of chunks in the old
			   song */
			player->cross_fade_chunks = cross_fade_position;
			player->cross_fading = true;
		}

		if (other_chunk != NULL) {
			chunk = player->pipe->Shift();
			assert(chunk != NULL);
			assert(chunk->other == NULL);

			/* don't send the tags of the new song (which
			   is being faded in) yet; postpone it until
			   the current song is faded out */
			player->cross_fade_tag =
				Tag::MergeReplace(player->cross_fade_tag,
						  other_chunk->tag);
			other_chunk->tag = NULL;

			if (std::isnan(pc->mixramp_delay_seconds)) {
				chunk->mix_ratio = ((float)cross_fade_position)
					     / player->cross_fade_chunks;
			} else {
				chunk->mix_ratio = nan("");
			}

			if (other_chunk->IsEmpty()) {
				/* the "other" chunk was a music_chunk
				   which had only a tag, but no music
				   data - we cannot cross-fade that;
				   but since this happens only at the
				   beginning of the new song, we can
				   easily recover by throwing it away
				   now */
				player_buffer->Return(other_chunk);
				other_chunk = NULL;
			}

			chunk->other = other_chunk;
		} else {
			/* there are not enough decoded chunks yet */

			dc->Lock();

			if (dc->IsIdle()) {
				/* the decoder isn't running, abort
				   cross fading */
				dc->Unlock();

				player->xfade = XFADE_DISABLED;
			} else {
				/* wait for the decoder */
				dc->Signal();
				dc->WaitForDecoder();
				dc->Unlock();

				return true;
			}
		}
	}

	if (chunk == NULL)
		chunk = player->pipe->Shift();

	assert(chunk != NULL);

	/* insert the postponed tag if cross-fading is finished */

	if (player->xfade != XFADE_ENABLED && player->cross_fade_tag != NULL) {
		chunk->tag = Tag::MergeReplace(chunk->tag,
					       player->cross_fade_tag);
		player->cross_fade_tag = NULL;
	}

	/* play the current chunk */

	Error error;
	if (!play_chunk(player->pc, player->song, chunk,
			player->play_audio_format, error)) {
		g_warning("%s", error.GetMessage());

		player_buffer->Return(chunk);

		pc->Lock();

		pc->SetError(PLAYER_ERROR_OUTPUT, std::move(error));

		/* pause: the user may resume playback as soon as an
		   audio output becomes available */
		pc->state = PLAYER_STATE_PAUSE;
		player->paused = true;

		pc->Unlock();

		idle_add(IDLE_PLAYER);

		return false;
	}

	/* this formula should prevent that the decoder gets woken up
	   with each chunk; it is more efficient to make it decode a
	   larger block at a time */
	dc->Lock();
	if (!dc->IsIdle() &&
	    dc->pipe->GetSize() <= (pc->buffered_before_play +
				    player_buffer->GetSize() * 3) / 4)
		dc->Signal();
	dc->Unlock();

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
	player->xfade = XFADE_UNKNOWN;

	char *uri = player->song->GetURI();
	g_message("played \"%s\"", uri);
	g_free(uri);

	delete player->pipe;
	player->pipe = player->dc->pipe;

	audio_output_all_song_border();

	if (!player_wait_for_decoder(player))
		return false;

	struct player_control *const pc = player->pc;
	pc->Lock();

	const bool border_pause = pc->border_pause;
	if (border_pause) {
		player->paused = true;
		pc->state = PLAYER_STATE_PAUSE;
	}

	pc->Unlock();

	if (border_pause)
		idle_add(IDLE_PLAYER);

	return true;
}

/*
 * The main loop of the player thread, during playback.  This is
 * basically a state machine, which multiplexes data between the
 * decoder thread and the output threads.
 */
static void do_play(struct player_control *pc, struct decoder_control *dc)
{
	player player(pc, dc);

	pc->Unlock();

	player.pipe = new MusicPipe();

	player_dc_start(&player, *player.pipe);
	if (!player_wait_for_decoder(&player)) {
		assert(player.song == NULL);

		player_dc_stop(&player);
		player_command_finished(pc);
		delete player.pipe;
		GlobalEvents::Emit(GlobalEvents::PLAYLIST);
		pc->Lock();
		return;
	}

	pc->Lock();
	pc->state = PLAYER_STATE_PLAY;

	if (pc->command == PLAYER_COMMAND_SEEK)
		player.elapsed_time = pc->seek_where;

	player_command_finished_locked(pc);

	while (true) {
		player_process_command(&player);
		if (pc->command == PLAYER_COMMAND_STOP ||
		    pc->command == PLAYER_COMMAND_EXIT ||
		    pc->command == PLAYER_COMMAND_CLOSE_AUDIO) {
			pc->Unlock();
			audio_output_all_cancel();
			break;
		}

		pc->Unlock();

		if (player.buffering) {
			/* buffering at the start of the song - wait
			   until the buffer is large enough, to
			   prevent stuttering on slow machines */

			if (player.pipe->GetSize() < pc->buffered_before_play &&
			    !dc->LockIsIdle()) {
				/* not enough decoded buffer space yet */

				if (!player.paused &&
				    player.output_open &&
				    audio_output_all_check() < 4 &&
				    !player_send_silence(&player))
					break;

				dc->Lock();
				/* XXX race condition: check decoder again */
				dc->WaitForDecoder();
				dc->Unlock();
				pc->Lock();
				continue;
			} else {
				/* buffering is complete */
				player.buffering = false;
			}
		}

		if (player.decoder_starting) {
			/* wait until the decoder is initialized completely */

			if (!player_check_decoder_startup(&player))
				break;

			pc->Lock();
			continue;
		}

#ifndef NDEBUG
		/*
		music_pipe_check_format(&play_audio_format,
					player.next_song_chunk,
					&dc->out_audio_format);
		*/
#endif

		if (dc->LockIsIdle() && player.queued &&
		    dc->pipe == player.pipe) {
			/* the decoder has finished the current song;
			   make it decode the next song */

			assert(dc->pipe == NULL || dc->pipe == player.pipe);

			player_dc_start(&player, *new MusicPipe());
		}

		if (/* no cross-fading if MPD is going to pause at the
		       end of the current song */
		    !pc->border_pause &&
		    player_dc_at_next_song(&player) &&
		    player.xfade == XFADE_UNKNOWN &&
		    !dc->LockIsStarting()) {
			/* enable cross fading in this song?  if yes,
			   calculate how many chunks will be required
			   for it */
			player.cross_fade_chunks =
				cross_fade_calc(pc->cross_fade_seconds, dc->total_time,
						pc->mixramp_db,
						pc->mixramp_delay_seconds,
						dc->replay_gain_db,
						dc->replay_gain_prev_db,
						dc->mixramp_start,
						dc->mixramp_prev_end,
						dc->out_audio_format,
						player.play_audio_format,
						player_buffer->GetSize() -
						pc->buffered_before_play);
			if (player.cross_fade_chunks > 0) {
				player.xfade = XFADE_ENABLED;
				player.cross_fading = false;
			} else
				/* cross fading is disabled or the
				   next song is too short */
				player.xfade = XFADE_DISABLED;
		}

		if (player.paused) {
			pc->Lock();

			if (pc->command == PLAYER_COMMAND_NONE)
				pc->Wait();
			continue;
		} else if (!player.pipe->IsEmpty()) {
			/* at least one music chunk is ready - send it
			   to the audio output */

			play_next_chunk(&player);
		} else if (audio_output_all_check() > 0) {
			/* not enough data from decoder, but the
			   output thread is still busy, so it's
			   okay */

			/* XXX synchronize in a better way */
			g_usleep(10000);
		} else if (player_dc_at_next_song(&player)) {
			/* at the beginning of a new song */

			if (!player_song_border(&player))
				break;
		} else if (dc->LockIsIdle()) {
			/* check the size of the pipe again, because
			   the decoder thread may have added something
			   since we last checked */
			if (player.pipe->IsEmpty()) {
				/* wait for the hardware to finish
				   playback */
				audio_output_all_drain();
				break;
			}
		} else if (player.output_open) {
			/* the decoder is too busy and hasn't provided
			   new PCM data in time: send silence (if the
			   output pipe is empty) */
			if (!player_send_silence(&player))
				break;
		}

		pc->Lock();
	}

	player_dc_stop(&player);

	player.pipe->Clear(*player_buffer);
	delete player.pipe;

	delete player.cross_fade_tag;

	if (player.song != NULL)
		player.song->Free();

	pc->Lock();

	if (player.queued) {
		assert(pc->next_song != NULL);
		pc->next_song->Free();
		pc->next_song = NULL;
	}

	pc->state = PLAYER_STATE_STOP;

	pc->Unlock();

	GlobalEvents::Emit(GlobalEvents::PLAYLIST);

	pc->Lock();
}

static gpointer
player_task(gpointer arg)
{
	struct player_control *pc = (struct player_control *)arg;

	struct decoder_control *dc = new decoder_control();
	decoder_thread_start(dc);

	player_buffer = new MusicBuffer(pc->buffer_chunks);

	pc->Lock();

	while (1) {
		switch (pc->command) {
		case PLAYER_COMMAND_SEEK:
		case PLAYER_COMMAND_QUEUE:
			assert(pc->next_song != NULL);

			do_play(pc, dc);
			break;

		case PLAYER_COMMAND_STOP:
			pc->Unlock();
			audio_output_all_cancel();
			pc->Lock();

			/* fall through */

		case PLAYER_COMMAND_PAUSE:
			if (pc->next_song != NULL) {
				pc->next_song->Free();
				pc->next_song = NULL;
			}

			player_command_finished_locked(pc);
			break;

		case PLAYER_COMMAND_CLOSE_AUDIO:
			pc->Unlock();

			audio_output_all_release();

			pc->Lock();
			player_command_finished_locked(pc);

#ifndef NDEBUG
			/* in the DEBUG build, check for leaked
			   music_chunk objects by freeing the
			   music_buffer */
			delete player_buffer;
			player_buffer = new MusicBuffer(pc->buffer_chunks);
#endif

			break;

		case PLAYER_COMMAND_UPDATE_AUDIO:
			pc->Unlock();
			audio_output_all_enable_disable();
			pc->Lock();
			player_command_finished_locked(pc);
			break;

		case PLAYER_COMMAND_EXIT:
			pc->Unlock();

			dc->Quit();
			delete dc;
			audio_output_all_close();
			delete player_buffer;

			player_command_finished(pc);
			return NULL;

		case PLAYER_COMMAND_CANCEL:
			if (pc->next_song != NULL) {
				pc->next_song->Free();
				pc->next_song = NULL;
			}

			player_command_finished_locked(pc);
			break;

		case PLAYER_COMMAND_REFRESH:
			/* no-op when not playing */
			player_command_finished_locked(pc);
			break;

		case PLAYER_COMMAND_NONE:
			pc->Wait();
			break;
		}
	}
}

void
player_create(struct player_control *pc)
{
	assert(pc->thread == NULL);

#if GLIB_CHECK_VERSION(2,32,0)
	pc->thread = g_thread_new("player", player_task, pc);
#else
	GError *e = NULL;
	pc->thread = g_thread_create(player_task, pc, true, &e);
	if (pc->thread == NULL)
		FatalError("Failed to spawn player task", e);
#endif
}
