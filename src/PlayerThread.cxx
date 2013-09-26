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
	player_control &pc;

	decoder_control &dc;

	MusicBuffer &buffer;

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

	player(player_control &_pc, decoder_control &_dc,
	       MusicBuffer &_buffer)
		:pc(_pc), dc(_dc), buffer(_buffer),
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

	void ClearAndDeletePipe() {
		pipe->Clear(buffer);
		delete pipe;
	}

	void ClearAndReplacePipe(MusicPipe *_pipe) {
		ClearAndDeletePipe();
		pipe = _pipe;
	}

	void ReplacePipe(MusicPipe *_pipe) {
		delete pipe;
		pipe = _pipe;
	}

	/**
	 * Start the decoder.
	 *
	 * Player lock is not held.
	 */
	void StartDecoder(MusicPipe &pipe);

	/**
	 * The decoder has acknowledged the "START" command (see
	 * player::WaitForDecoder()).  This function checks if the decoder
	 * initialization has completed yet.
	 *
	 * The player lock is not held.
	 */
	bool CheckDecoderStartup();

	/**
	 * Stop the decoder and clears (and frees) its music pipe.
	 *
	 * Player lock is not held.
	 */
	void StopDecoder();

	/**
	 * Is the decoder still busy on the same song as the player?
	 *
	 * Note: this function does not check if the decoder is already
	 * finished.
	 */
	gcc_pure
	bool IsDecoderAtCurrentSong() const {
		assert(pipe != nullptr);

		return dc.pipe == pipe;
	}

	/**
	 * Returns true if the decoder is decoding the next song (or has begun
	 * decoding it, or has finished doing it), and the player hasn't
	 * switched to that song yet.
	 */
	gcc_pure
	bool IsDecoderAtNextSong() const {
		return dc.pipe != nullptr && !IsDecoderAtCurrentSong();
	}

	/**
	 * This is the handler for the #PLAYER_COMMAND_SEEK command.
	 *
	 * The player lock is not held.
	 */
	bool SeekDecoder();

	/**
	 * After the decoder has been started asynchronously, wait for
	 * the "START" command to finish.  The decoder may not be
	 * initialized yet, i.e. there is no audio_format information
	 * yet.
	 *
	 * The player lock is not held.
	 */
	bool WaitForDecoder();

	/**
	 * Wrapper for audio_output_all_open().  Upon failure, it pauses the
	 * player.
	 *
	 * @return true on success
	 */
	bool OpenOutput();

	/**
	 * Obtains the next chunk from the music pipe, optionally applies
	 * cross-fading, and sends it to all audio outputs.
	 *
	 * @return true on success, false on error (playback will be stopped)
	 */
	bool PlayNextChunk();

	/**
	 * Sends a chunk of silence to the audio outputs.  This is
	 * called when there is not enough decoded data in the pipe
	 * yet, to prevent underruns in the hardware buffers.
	 *
	 * The player lock is not held.
	 */
	bool SendSilence();

	/**
	 * Player lock must be held before calling.
	 */
	void ProcessCommand();

	/**
	 * This is called at the border between two songs: the audio output
	 * has consumed all chunks of the current song, and we should start
	 * sending chunks from the next one.
	 *
	 * The player lock is not held.
	 *
	 * @return true on success, false on error (playback will be stopped)
	 */
	bool SongBorder();

	/*
	 * The main loop of the player thread, during playback.  This
	 * is basically a state machine, which multiplexes data
	 * between the decoder thread and the output threads.
	 */
	void Run();
};

static void
player_command_finished_locked(player_control &pc)
{
	assert(pc.command != PLAYER_COMMAND_NONE);

	pc.command = PLAYER_COMMAND_NONE;
	pc.ClientSignal();
}

static void
player_command_finished(player_control &pc)
{
	pc.Lock();
	player_command_finished_locked(pc);
	pc.Unlock();
}

void
player::StartDecoder(MusicPipe &_pipe)
{
	assert(queued || pc.command == PLAYER_COMMAND_SEEK);
	assert(pc.next_song != NULL);

	unsigned start_ms = pc.next_song->start_ms;
	if (pc.command == PLAYER_COMMAND_SEEK)
		start_ms += (unsigned)(pc.seek_where * 1000);

	dc.Start(pc.next_song->DupDetached(),
		 start_ms, pc.next_song->end_ms,
		 buffer, _pipe);
}

void
player::StopDecoder()
{
	dc.Stop();

	if (dc.pipe != NULL) {
		/* clear and free the decoder pipe */

		dc.pipe->Clear(buffer);

		if (dc.pipe != pipe)
			delete dc.pipe;

		dc.pipe = NULL;
	}
}

bool
player::WaitForDecoder()
{
	assert(queued || pc.command == PLAYER_COMMAND_SEEK);
	assert(pc.next_song != NULL);

	queued = false;

	Error error = dc.LockGetError();
	if (error.IsDefined()) {
		pc.Lock();
		pc.SetError(PLAYER_ERROR_DECODER, std::move(error));

		pc.next_song->Free();
		pc.next_song = NULL;

		pc.Unlock();

		return false;
	}

	if (song != nullptr)
		song->Free();

	song = pc.next_song;
	elapsed_time = 0.0;

	/* set the "starting" flag, which will be cleared by
	   player_check_decoder_startup() */
	decoder_starting = true;

	pc.Lock();

	/* update player_control's song information */
	pc.total_time = pc.next_song->GetDuration();
	pc.bit_rate = 0;
	pc.audio_format.Clear();

	/* clear the queued song */
	pc.next_song = NULL;

	pc.Unlock();

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

bool
player::OpenOutput()
{
	assert(play_audio_format.IsDefined());
	assert(pc.state == PLAYER_STATE_PLAY ||
	       pc.state == PLAYER_STATE_PAUSE);

	Error error;
	if (audio_output_all_open(play_audio_format, buffer, error)) {
		output_open = true;
		paused = false;

		pc.Lock();
		pc.state = PLAYER_STATE_PLAY;
		pc.Unlock();

		idle_add(IDLE_PLAYER);

		return true;
	} else {
		g_warning("%s", error.GetMessage());

		output_open = false;

		/* pause: the user may resume playback as soon as an
		   audio output becomes available */
		paused = true;

		pc.Lock();
		pc.SetError(PLAYER_ERROR_OUTPUT, std::move(error));
		pc.state = PLAYER_STATE_PAUSE;
		pc.Unlock();

		idle_add(IDLE_PLAYER);

		return false;
	}
}

bool
player::CheckDecoderStartup()
{
	assert(decoder_starting);

	dc.Lock();

	Error error = dc.GetError();
	if (error.IsDefined()) {
		/* the decoder failed */
		dc.Unlock();

		pc.Lock();
		pc.SetError(PLAYER_ERROR_DECODER, std::move(error));
		pc.Unlock();

		return false;
	} else if (!dc.IsStarting()) {
		/* the decoder is ready and ok */

		dc.Unlock();

		if (output_open &&
		    !audio_output_all_wait(&pc, 1))
			/* the output devices havn't finished playing
			   all chunks yet - wait for that */
			return true;

		pc.Lock();
		pc.total_time = real_song_duration(dc.song, dc.total_time);
		pc.audio_format = dc.in_audio_format;
		pc.Unlock();

		idle_add(IDLE_PLAYER);

		play_audio_format = dc.out_audio_format;
		decoder_starting = false;

		if (!paused && !OpenOutput()) {
			char *uri = dc.song->GetURI();
			g_warning("problems opening audio device "
				  "while playing \"%s\"", uri);
			g_free(uri);

			return true;
		}

		return true;
	} else {
		/* the decoder is not yet ready; wait
		   some more */
		dc.WaitForDecoder();
		dc.Unlock();

		return true;
	}
}

bool
player::SendSilence()
{
	assert(output_open);
	assert(play_audio_format.IsDefined());

	struct music_chunk *chunk = buffer.Allocate();
	if (chunk == NULL) {
		g_warning("Failed to allocate silence buffer");
		return false;
	}

#ifndef NDEBUG
	chunk->audio_format = play_audio_format;
#endif

	const size_t frame_size = play_audio_format.GetFrameSize();
	/* this formula ensures that we don't send
	   partial frames */
	unsigned num_frames = sizeof(chunk->data) / frame_size;

	chunk->times = -1.0; /* undefined time stamp */
	chunk->length = num_frames * frame_size;
	memset(chunk->data, 0, chunk->length);

	Error error;
	if (!audio_output_all_play(chunk, error)) {
		g_warning("%s", error.GetMessage());
		buffer.Return(chunk);
		return false;
	}

	return true;
}

inline bool
player::SeekDecoder()
{
	assert(pc.next_song != NULL);

	const unsigned start_ms = pc.next_song->start_ms;

	if (!dc.LockIsCurrentSong(pc.next_song)) {
		/* the decoder is already decoding the "next" song -
		   stop it and start the previous song again */

		StopDecoder();

		/* clear music chunks which might still reside in the
		   pipe */
		pipe->Clear(buffer);

		/* re-start the decoder */
		StartDecoder(*pipe);
		if (!WaitForDecoder()) {
			/* decoder failure */
			player_command_finished(pc);
			return false;
		}
	} else {
		if (!IsDecoderAtCurrentSong()) {
			/* the decoder is already decoding the "next" song,
			   but it is the same song file; exchange the pipe */
			ClearAndReplacePipe(dc.pipe);
		}

		pc.next_song->Free();
		pc.next_song = NULL;
		queued = false;
	}

	/* wait for the decoder to complete initialization */

	while (decoder_starting) {
		if (!CheckDecoderStartup()) {
			/* decoder failure */
			player_command_finished(pc);
			return false;
		}
	}

	/* send the SEEK command */

	double where = pc.seek_where;
	if (where > pc.total_time)
		where = pc.total_time - 0.1;
	if (where < 0.0)
		where = 0.0;

	if (!dc.Seek(where + start_ms / 1000.0)) {
		/* decoder failure */
		player_command_finished(pc);
		return false;
	}

	elapsed_time = where;

	player_command_finished(pc);

	xfade = XFADE_UNKNOWN;

	/* re-fill the buffer after seeking */
	buffering = true;

	audio_output_all_cancel();

	return true;
}

inline void
player::ProcessCommand()
{
	switch (pc.command) {
	case PLAYER_COMMAND_NONE:
	case PLAYER_COMMAND_STOP:
	case PLAYER_COMMAND_EXIT:
	case PLAYER_COMMAND_CLOSE_AUDIO:
		break;

	case PLAYER_COMMAND_UPDATE_AUDIO:
		pc.Unlock();
		audio_output_all_enable_disable();
		pc.Lock();
		player_command_finished_locked(pc);
		break;

	case PLAYER_COMMAND_QUEUE:
		assert(pc.next_song != NULL);
		assert(!queued);
		assert(!IsDecoderAtNextSong());

		queued = true;
		player_command_finished_locked(pc);
		break;

	case PLAYER_COMMAND_PAUSE:
		pc.Unlock();

		paused = !paused;
		if (paused) {
			audio_output_all_pause();
			pc.Lock();

			pc.state = PLAYER_STATE_PAUSE;
		} else if (!play_audio_format.IsDefined()) {
			/* the decoder hasn't provided an audio format
			   yet - don't open the audio device yet */
			pc.Lock();

			pc.state = PLAYER_STATE_PLAY;
		} else {
			OpenOutput();

			pc.Lock();
		}

		player_command_finished_locked(pc);
		break;

	case PLAYER_COMMAND_SEEK:
		pc.Unlock();
		SeekDecoder();
		pc.Lock();
		break;

	case PLAYER_COMMAND_CANCEL:
		if (pc.next_song == NULL) {
			/* the cancel request arrived too late, we're
			   already playing the queued song...  stop
			   everything now */
			pc.command = PLAYER_COMMAND_STOP;
			return;
		}

		if (IsDecoderAtNextSong()) {
			/* the decoder is already decoding the song -
			   stop it and reset the position */
			pc.Unlock();
			StopDecoder();
			pc.Lock();
		}

		pc.next_song->Free();
		pc.next_song = NULL;
		queued = false;
		player_command_finished_locked(pc);
		break;

	case PLAYER_COMMAND_REFRESH:
		if (output_open && !paused) {
			pc.Unlock();
			audio_output_all_check();
			pc.Lock();
		}

		pc.elapsed_time = audio_output_all_get_elapsed_time();
		if (pc.elapsed_time < 0.0)
			pc.elapsed_time = elapsed_time;

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
play_chunk(player_control &pc,
	   Song *song, struct music_chunk *chunk,
	   MusicBuffer &buffer,
	   const AudioFormat format,
	   Error &error)
{
	assert(chunk->CheckFormat(format));

	if (chunk->tag != NULL)
		update_song_tag(song, *chunk->tag);

	if (chunk->length == 0) {
		buffer.Return(chunk);
		return true;
	}

	pc.Lock();
	pc.bit_rate = chunk->bit_rate;
	pc.Unlock();

	/* send the chunk to the audio outputs */

	if (!audio_output_all_play(chunk, error))
		return false;

	pc.total_play_time += (double)chunk->length /
		format.GetTimeToSize();
	return true;
}

inline bool
player::PlayNextChunk()
{
	if (!audio_output_all_wait(&pc, 64))
		/* the output pipe is still large enough, don't send
		   another chunk */
		return true;

	unsigned cross_fade_position;
	struct music_chunk *chunk = NULL;
	if (xfade == XFADE_ENABLED && IsDecoderAtNextSong() &&
	    (cross_fade_position = pipe->GetSize()) <= cross_fade_chunks) {
		/* perform cross fade */
		music_chunk *other_chunk = dc.pipe->Shift();

		if (!cross_fading) {
			/* beginning of the cross fade - adjust
			   crossFadeChunks which might be bigger than
			   the remaining number of chunks in the old
			   song */
			cross_fade_chunks = cross_fade_position;
			cross_fading = true;
		}

		if (other_chunk != NULL) {
			chunk = pipe->Shift();
			assert(chunk != NULL);
			assert(chunk->other == NULL);

			/* don't send the tags of the new song (which
			   is being faded in) yet; postpone it until
			   the current song is faded out */
			cross_fade_tag =
				Tag::MergeReplace(cross_fade_tag,
						  other_chunk->tag);
			other_chunk->tag = NULL;

			if (std::isnan(pc.mixramp_delay_seconds)) {
				chunk->mix_ratio = ((float)cross_fade_position)
					     / cross_fade_chunks;
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
				buffer.Return(other_chunk);
				other_chunk = NULL;
			}

			chunk->other = other_chunk;
		} else {
			/* there are not enough decoded chunks yet */

			dc.Lock();

			if (dc.IsIdle()) {
				/* the decoder isn't running, abort
				   cross fading */
				dc.Unlock();

				xfade = XFADE_DISABLED;
			} else {
				/* wait for the decoder */
				dc.Signal();
				dc.WaitForDecoder();
				dc.Unlock();

				return true;
			}
		}
	}

	if (chunk == NULL)
		chunk = pipe->Shift();

	assert(chunk != NULL);

	/* insert the postponed tag if cross-fading is finished */

	if (xfade != XFADE_ENABLED && cross_fade_tag != nullptr) {
		chunk->tag = Tag::MergeReplace(chunk->tag, cross_fade_tag);
		cross_fade_tag = nullptr;
	}

	/* play the current chunk */

	Error error;
	if (!play_chunk(pc, song, chunk, buffer, play_audio_format, error)) {
		g_warning("%s", error.GetMessage());

		buffer.Return(chunk);

		pc.Lock();

		pc.SetError(PLAYER_ERROR_OUTPUT, std::move(error));

		/* pause: the user may resume playback as soon as an
		   audio output becomes available */
		pc.state = PLAYER_STATE_PAUSE;
		paused = true;

		pc.Unlock();

		idle_add(IDLE_PLAYER);

		return false;
	}

	/* this formula should prevent that the decoder gets woken up
	   with each chunk; it is more efficient to make it decode a
	   larger block at a time */
	dc.Lock();
	if (!dc.IsIdle() &&
	    dc.pipe->GetSize() <= (pc.buffered_before_play +
				   buffer.GetSize() * 3) / 4)
		dc.Signal();
	dc.Unlock();

	return true;
}

inline bool
player::SongBorder()
{
	xfade = XFADE_UNKNOWN;

	char *uri = song->GetURI();
	g_message("played \"%s\"", uri);
	g_free(uri);

	ReplacePipe(dc.pipe);

	audio_output_all_song_border();

	if (!WaitForDecoder())
		return false;

	pc.Lock();

	const bool border_pause = pc.border_pause;
	if (border_pause) {
		paused = true;
		pc.state = PLAYER_STATE_PAUSE;
	}

	pc.Unlock();

	if (border_pause)
		idle_add(IDLE_PLAYER);

	return true;
}

inline void
player::Run()
{
	pc.Unlock();

	pipe = new MusicPipe();

	StartDecoder(*pipe);
	if (!WaitForDecoder()) {
		assert(song == NULL);

		StopDecoder();
		player_command_finished(pc);
		delete pipe;
		GlobalEvents::Emit(GlobalEvents::PLAYLIST);
		pc.Lock();
		return;
	}

	pc.Lock();
	pc.state = PLAYER_STATE_PLAY;

	if (pc.command == PLAYER_COMMAND_SEEK)
		elapsed_time = pc.seek_where;

	player_command_finished_locked(pc);

	while (true) {
		ProcessCommand();
		if (pc.command == PLAYER_COMMAND_STOP ||
		    pc.command == PLAYER_COMMAND_EXIT ||
		    pc.command == PLAYER_COMMAND_CLOSE_AUDIO) {
			pc.Unlock();
			audio_output_all_cancel();
			break;
		}

		pc.Unlock();

		if (buffering) {
			/* buffering at the start of the song - wait
			   until the buffer is large enough, to
			   prevent stuttering on slow machines */

			if (pipe->GetSize() < pc.buffered_before_play &&
			    !dc.LockIsIdle()) {
				/* not enough decoded buffer space yet */

				if (!paused && output_open &&
				    audio_output_all_check() < 4 &&
				    !SendSilence())
					break;

				dc.Lock();
				/* XXX race condition: check decoder again */
				dc.WaitForDecoder();
				dc.Unlock();
				pc.Lock();
				continue;
			} else {
				/* buffering is complete */
				buffering = false;
			}
		}

		if (decoder_starting) {
			/* wait until the decoder is initialized completely */

			if (!CheckDecoderStartup())
				break;

			pc.Lock();
			continue;
		}

#ifndef NDEBUG
		/*
		music_pipe_check_format(&play_audio_format,
					next_song_chunk,
					&dc.out_audio_format);
		*/
#endif

		if (dc.LockIsIdle() && queued && dc.pipe == pipe) {
			/* the decoder has finished the current song;
			   make it decode the next song */

			assert(dc.pipe == NULL || dc.pipe == pipe);

			StartDecoder(*new MusicPipe());
		}

		if (/* no cross-fading if MPD is going to pause at the
		       end of the current song */
		    !pc.border_pause &&
		    IsDecoderAtNextSong() &&
		    xfade == XFADE_UNKNOWN &&
		    !dc.LockIsStarting()) {
			/* enable cross fading in this song?  if yes,
			   calculate how many chunks will be required
			   for it */
			cross_fade_chunks =
				cross_fade_calc(pc.cross_fade_seconds, dc.total_time,
						pc.mixramp_db,
						pc.mixramp_delay_seconds,
						dc.replay_gain_db,
						dc.replay_gain_prev_db,
						dc.mixramp_start,
						dc.mixramp_prev_end,
						dc.out_audio_format,
						play_audio_format,
						buffer.GetSize() -
						pc.buffered_before_play);
			if (cross_fade_chunks > 0) {
				xfade = XFADE_ENABLED;
				cross_fading = false;
			} else
				/* cross fading is disabled or the
				   next song is too short */
				xfade = XFADE_DISABLED;
		}

		if (paused) {
			pc.Lock();

			if (pc.command == PLAYER_COMMAND_NONE)
				pc.Wait();
			continue;
		} else if (!pipe->IsEmpty()) {
			/* at least one music chunk is ready - send it
			   to the audio output */

			PlayNextChunk();
		} else if (audio_output_all_check() > 0) {
			/* not enough data from decoder, but the
			   output thread is still busy, so it's
			   okay */

			/* XXX synchronize in a better way */
			g_usleep(10000);
		} else if (IsDecoderAtNextSong()) {
			/* at the beginning of a new song */

			if (!SongBorder())
				break;
		} else if (dc.LockIsIdle()) {
			/* check the size of the pipe again, because
			   the decoder thread may have added something
			   since we last checked */
			if (pipe->IsEmpty()) {
				/* wait for the hardware to finish
				   playback */
				audio_output_all_drain();
				break;
			}
		} else if (output_open) {
			/* the decoder is too busy and hasn't provided
			   new PCM data in time: send silence (if the
			   output pipe is empty) */
			if (!SendSilence())
				break;
		}

		pc.Lock();
	}

	StopDecoder();

	ClearAndDeletePipe();

	delete cross_fade_tag;

	if (song != nullptr)
		song->Free();

	pc.Lock();

	if (queued) {
		assert(pc.next_song != NULL);
		pc.next_song->Free();
		pc.next_song = NULL;
	}

	pc.state = PLAYER_STATE_STOP;

	pc.Unlock();

	GlobalEvents::Emit(GlobalEvents::PLAYLIST);

	pc.Lock();
}

static void
do_play(player_control &pc, decoder_control &dc,
	MusicBuffer &buffer)
{
	player player(pc, dc, buffer);
	player.Run();
}

static gpointer
player_task(gpointer arg)
{
	player_control &pc = *(player_control *)arg;

	decoder_control dc;
	decoder_thread_start(&dc);

	MusicBuffer buffer(pc.buffer_chunks);

	pc.Lock();

	while (1) {
		switch (pc.command) {
		case PLAYER_COMMAND_SEEK:
		case PLAYER_COMMAND_QUEUE:
			assert(pc.next_song != NULL);

			do_play(pc, dc, buffer);
			break;

		case PLAYER_COMMAND_STOP:
			pc.Unlock();
			audio_output_all_cancel();
			pc.Lock();

			/* fall through */

		case PLAYER_COMMAND_PAUSE:
			if (pc.next_song != NULL) {
				pc.next_song->Free();
				pc.next_song = NULL;
			}

			player_command_finished_locked(pc);
			break;

		case PLAYER_COMMAND_CLOSE_AUDIO:
			pc.Unlock();

			audio_output_all_release();

			pc.Lock();
			player_command_finished_locked(pc);

			assert(buffer.IsEmptyUnsafe());

			break;

		case PLAYER_COMMAND_UPDATE_AUDIO:
			pc.Unlock();
			audio_output_all_enable_disable();
			pc.Lock();
			player_command_finished_locked(pc);
			break;

		case PLAYER_COMMAND_EXIT:
			pc.Unlock();

			dc.Quit();

			audio_output_all_close();

			player_command_finished(pc);
			return NULL;

		case PLAYER_COMMAND_CANCEL:
			if (pc.next_song != NULL) {
				pc.next_song->Free();
				pc.next_song = NULL;
			}

			player_command_finished_locked(pc);
			break;

		case PLAYER_COMMAND_REFRESH:
			/* no-op when not playing */
			player_command_finished_locked(pc);
			break;

		case PLAYER_COMMAND_NONE:
			pc.Wait();
			break;
		}
	}
}

void
player_create(player_control &pc)
{
	assert(pc.thread == NULL);

#if GLIB_CHECK_VERSION(2,32,0)
	pc.thread = g_thread_new("player", player_task, &pc);
#else
	GError *e = NULL;
	pc.thread = g_thread_create(player_task, &pc, true, &e);
	if (pc.thread == NULL)
		FatalError("Failed to spawn player task", e);
#endif
}
