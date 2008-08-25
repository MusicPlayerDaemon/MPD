/*
 * This file is originally from JACK Audio Connection Kit
 *
 * Copyright (C) 2000 Paul Davis
 * Copyright (C) 2003 Rohan Drape
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * ISO/POSIX C version of Paul Davis's lock free ringbuffer C++ code.
 * This is safe for the case of one read thread and one write thread.
 */

#include "ringbuf.h"
#include "utils.h"

#define advance_ptr(ptr,cnt,mask)  ptr = (ptr + cnt) & mask

/*
 * Create a new ringbuffer to hold at least `sz' bytes of data. The
 * actual buffer size is rounded up to the next power of two.
 */
struct ringbuf *ringbuf_create(size_t sz)
{
	struct ringbuf *rb = xmalloc(sizeof(struct ringbuf));
	size_t power_of_two;

	for (power_of_two = 1; (size_t)(1 << power_of_two) < sz; power_of_two++)
		/* next power_of_two... */;

	rb->size = 1 << power_of_two;
	rb->size_mask = rb->size;
	rb->size_mask -= 1;
	rb->buf = xmalloc(rb->size);
	ringbuf_reset(rb);

	return rb;
}

/* Free all data associated with the ringbuffer `rb'. */
void ringbuf_free(struct ringbuf * rb)
{
	free(rb->buf);
	free(rb);
}

/* Reset the read and write pointers to zero. This is not thread safe. */
void ringbuf_reset(struct ringbuf * rb)
{
	rb->read_ptr = 0;
	rb->write_ptr = 0;
}

/* Reset the read and write pointers, thread-safe iff called only by writer */
void ringbuf_writer_reset(struct ringbuf * rb)
{
	rb->write_ptr = rb->read_ptr;
}

/* Reset the read and write pointers, thread-safe iff called only by reader */
void ringbuf_reader_reset(struct ringbuf * rb)
{
	rb->read_ptr = rb->write_ptr;
}

/*
 * Return the number of bytes available for reading.  This is the
 * number of bytes in front of the read pointer and behind the write
 * pointer.
 */
size_t ringbuf_read_space(const struct ringbuf * rb)
{
	size_t w = rb->write_ptr;
	size_t r = rb->read_ptr;

	if (w > r)
		return w - r;
	else
		return (w - r + rb->size) & rb->size_mask;
}

/*
 * Return the number of bytes available for writing.  This is the
 * number of bytes in front of the write pointer and behind the read
 * pointer.
 */
size_t ringbuf_write_space(const struct ringbuf * rb)
{
	size_t w = rb->write_ptr;
	size_t r = rb->read_ptr;

	if (w > r)
		return ((r - w + rb->size) & rb->size_mask) - 1;
	else if (w < r)
		return (r - w) - 1;
	else
		return rb->size - 1;
}

/*
 * The copying data reader.  Copy at most `cnt' bytes from `rb' to
 * `dest'.  Returns the actual number of bytes copied.
 */
size_t ringbuf_read(struct ringbuf * rb, void *dest, size_t cnt)
{
	size_t free_cnt;
	size_t cnt2;
	size_t to_read;
	size_t n1, n2;

	if ((free_cnt = ringbuf_read_space(rb)) == 0)
		return 0;

	to_read = cnt > free_cnt ? free_cnt : cnt;
	cnt2 = rb->read_ptr + to_read;

	if (cnt2 > rb->size) {
		n1 = rb->size - rb->read_ptr;
		n2 = cnt2 & rb->size_mask;
	} else {
		n1 = to_read;
		n2 = 0;
	}

	memcpy(dest, rb->buf + rb->read_ptr, n1);
	ringbuf_read_advance(rb, n1);

	if (n2) {
		memcpy((char*)dest + n1, rb->buf + rb->read_ptr, n2);
		ringbuf_read_advance(rb, n2);
	}

	return to_read;
}

/*
 * The copying data reader w/o read pointer advance.  Copy at most
 * `cnt' bytes from `rb' to `dest'.  Returns the actual number of bytes
 * copied.
 */
size_t ringbuf_peek(struct ringbuf * rb, void *dest, size_t cnt)
{
	size_t free_cnt;
	size_t cnt2;
	size_t to_read;
	size_t n1, n2;
	size_t tmp_read_ptr = rb->read_ptr;

	if ((free_cnt = ringbuf_read_space(rb)) == 0)
		return 0;

	to_read = cnt > free_cnt ? free_cnt : cnt;
	cnt2 = tmp_read_ptr + to_read;

	if (cnt2 > rb->size) {
		n1 = rb->size - tmp_read_ptr;
		n2 = cnt2 & rb->size_mask;
	} else {
		n1 = to_read;
		n2 = 0;
	}

	memcpy(dest, rb->buf + tmp_read_ptr, n1);
	advance_ptr(tmp_read_ptr, n1, rb->size_mask);

	if (n2) {
		memcpy((char*)dest + n1, rb->buf + tmp_read_ptr, n2);
		advance_ptr(tmp_read_ptr, n2, rb->size_mask);
	}

	return to_read;
}

/*
 * The copying data writer.  Copy at most `cnt' bytes to `rb' from
 * `src'.  Returns the actual number of bytes copied.
 */
size_t ringbuf_write(struct ringbuf * rb, const void *src, size_t cnt)
{
	size_t free_cnt;
	size_t cnt2;
	size_t to_write;
	size_t n1, n2;

	if ((free_cnt = ringbuf_write_space(rb)) == 0)
		return 0;

	to_write = cnt > free_cnt ? free_cnt : cnt;
	cnt2 = rb->write_ptr + to_write;

	if (cnt2 > rb->size) {
		n1 = rb->size - rb->write_ptr;
		n2 = cnt2 & rb->size_mask;
	} else {
		n1 = to_write;
		n2 = 0;
	}

	memcpy(rb->buf + rb->write_ptr, src, n1);
	ringbuf_write_advance(rb, n1);

	if (n2) {
		memcpy(rb->buf + rb->write_ptr, (const char*)src + n1, n2);
		ringbuf_write_advance(rb, n2);
	}

	return to_write;
}

/* Advance the read pointer `cnt' places. */
void ringbuf_read_advance(struct ringbuf * rb, size_t cnt)
{
	advance_ptr(rb->read_ptr, cnt, rb->size_mask);
}

/* Advance the write pointer `cnt' places. */
void ringbuf_write_advance(struct ringbuf * rb, size_t cnt)
{
	advance_ptr(rb->write_ptr, cnt, rb->size_mask);
}

/*
 * The non-copying data reader.  `vec' is an array of two places.  Set
 * the values at `vec' to hold the current readable data at `rb'.  If
 * the readable data is in one segment the second segment has zero
 * length.
 */
size_t ringbuf_get_read_vector(const struct ringbuf * rb, struct iovec * vec)
{
	size_t free_cnt;
	size_t cnt2;
	size_t w = rb->write_ptr;
	size_t r = rb->read_ptr;

	if (w > r)
		free_cnt = w - r;
	else
		free_cnt = (w - r + rb->size) & rb->size_mask;

	cnt2 = r + free_cnt;

	if (cnt2 > rb->size) {
		/*
		 * Two part vector: the rest of the buffer after the current
		 * write ptr, plus some from the start of the buffer.
		 */
		vec[0].iov_base = rb->buf + r;
		vec[0].iov_len = rb->size - r;
		vec[1].iov_base = rb->buf;
		vec[1].iov_len = cnt2 & rb->size_mask;
	} else {
		/* Single part vector: just the rest of the buffer */
		vec[0].iov_base = rb->buf + r;
		vec[0].iov_len = free_cnt;
		vec[1].iov_len = 0;
	}
	return vec[0].iov_len + vec[1].iov_len;
}

/*
 * The non-copying data writer.  `vec' is an array of two places.  Set
 * the values at `vec' to hold the current writeable data at `rb'.  If
 * the writeable data is in one segment the second segment has zero
 * length.
 */
size_t ringbuf_get_write_vector(const struct ringbuf * rb, struct iovec * vec)
{
	size_t free_cnt;
	size_t cnt2;
	size_t w = rb->write_ptr;
	size_t r = rb->read_ptr;

	if (w > r)
		free_cnt = ((r - w + rb->size) & rb->size_mask) - 1;
	else if (w < r)
		free_cnt = (r - w) - 1;
	else
		free_cnt = rb->size - 1;

	cnt2 = w + free_cnt;

	if (cnt2 > rb->size) {
		/*
		 * Two part vector: the rest of the buffer after the current
		 * write ptr, plus some from the start of the buffer.
		 */
		vec[0].iov_base = rb->buf + w;
		vec[0].iov_len = rb->size - w;
		vec[1].iov_base = rb->buf;
		vec[1].iov_len = cnt2 & rb->size_mask;
	} else {
		vec[0].iov_base = rb->buf + w;
		vec[0].iov_len = free_cnt;
		vec[1].iov_len = 0;
	}
	return vec[0].iov_len + vec[1].iov_len;
}

