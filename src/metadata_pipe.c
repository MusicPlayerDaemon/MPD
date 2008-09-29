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

#include "metadata_pipe.h"
#include "ringbuf.h"
#include "decode.h"
#include "os_compat.h"
#include "log.h"
#include "outputBuffer.h"
#include "gcc.h"

static struct ringbuf *mp;

/* Each one of these is a packet inside the metadata pipe */
struct tag_container {
	float metadata_time;
	uint8_t seq; /* ob.seq_decoder at time of metadata_pipe_send() */
	struct mpd_tag *tag; /* our payload */
};

/*
 * We have two readers even though ringbuf was designed for one (locklessly),
 * so we will use a lock to allow readers to safely read.  Writing is only
 * done from one thread, so it will never block or clobber.
 */
static pthread_mutex_t read_lock = PTHREAD_MUTEX_INITIALIZER;
static struct mpd_tag *current_tag; /* requires read_lock for both r/w access */

static void metadata_pipe_finish(void)
{
	ringbuf_free(mp);
	if (current_tag)
		tag_free(current_tag);
}

void init_metadata_pipe(void)
{
	mp = ringbuf_create(sizeof(struct tag_container) * 16);
	atexit(metadata_pipe_finish);
}

void metadata_pipe_send(struct mpd_tag *tag, float metadata_time)
{
	struct tag_container tc;
	size_t written;

	assert(pthread_equal(pthread_self(), dc.thread));

	if (mpd_unlikely(ringbuf_write_space(mp)
	                 < sizeof(struct tag_container))) {
		DEBUG("metadata_pipe: insufficient buffer space, dropping\n");
		tag_free(tag);
		return;
	}

	tc.tag = tag;
	tc.metadata_time = metadata_time;
	tc.seq = ob_get_decoder_sequence();
	written = ringbuf_write(mp, &tc, sizeof(struct tag_container));
	assert(written == sizeof(struct tag_container));
}

struct mpd_tag * metadata_pipe_recv(void)
{
	struct tag_container tc;
	size_t r;
	static const size_t uint8_t_max = 255; /* XXX CLEANUP */
	uint8_t expect_seq = ob_get_player_sequence();
	unsigned long current_time = ob_get_elapsed_time();
	struct mpd_tag *tag = NULL;

	if (pthread_mutex_trylock(&read_lock) == EBUSY)
		return NULL;
retry:
	if (!(r = ringbuf_peek(mp, &tc, sizeof(struct tag_container))))
		goto out;

	assert(r == sizeof(struct tag_container));
	assert(tc.tag);
	if (expect_seq == tc.seq) {
		if (current_time < tc.metadata_time)
			goto out; /* not ready for tag yet */
		if (tag_equal(tc.tag, current_tag)) {
			tag_free(tc.tag);
			ringbuf_read_advance(mp, sizeof(struct tag_container));
			goto out; /* nothing changed, don't bother */
		}
		tag = tag_dup(tc.tag);
		if (current_tag)
			tag_free(current_tag);
		current_tag = tc.tag;
		ringbuf_read_advance(mp, sizeof(struct tag_container));
	} else if (expect_seq > tc.seq ||
	           (!expect_seq && tc.seq == uint8_t_max)) {
		DEBUG("metadata_pipe: reader is ahead of writer\n");
		tag_free(tc.tag);
		ringbuf_read_advance(mp, sizeof(struct tag_container));
		goto retry; /* read and skip packets */
	} /* else not ready for tag yet */
out:
	pthread_mutex_unlock(&read_lock);
	return tag;
}

struct mpd_tag *metadata_pipe_current(void)
{
	struct mpd_tag *tag;

	assert(! pthread_equal(pthread_self(), dc.thread));
	if (pthread_mutex_trylock(&read_lock) == EBUSY)
		return NULL;
	tag = current_tag ? tag_dup(current_tag) : NULL;
	pthread_mutex_unlock(&read_lock);

	return tag;
}

void metadata_pipe_clear(void)
{
	struct tag_container tc;
	size_t r;

	pthread_mutex_lock(&read_lock);

	while ((r = ringbuf_read(mp, &tc, sizeof(struct tag_container)))) {
		assert(r == sizeof(struct tag_container));
		tag_free(tc.tag);
	}

	if (current_tag) {
		tag_free(current_tag);
		current_tag = NULL;
	}

	pthread_mutex_unlock(&read_lock);
}
