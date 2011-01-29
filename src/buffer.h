/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_MUSIC_BUFFER_H
#define MPD_MUSIC_BUFFER_H

/**
 * An allocator for #music_chunk objects.
 */
struct music_buffer;

/**
 * Creates a new #music_buffer object.
 *
 * @param num_chunks the number of #music_chunk reserved in this
 * buffer
 */
struct music_buffer *
music_buffer_new(unsigned num_chunks);

/**
 * Frees the #music_buffer object
 */
void
music_buffer_free(struct music_buffer *buffer);

/**
 * Returns the total number of reserved chunks in this buffer.  This
 * is the same value which was passed to the constructor
 * music_buffer_new().
 */
unsigned
music_buffer_size(const struct music_buffer *buffer);

/**
 * Allocates a chunk from the buffer.  When it is not used anymore,
 * call music_buffer_return().
 *
 * @return an empty chunk or NULL if there are no chunks available
 */
struct music_chunk *
music_buffer_allocate(struct music_buffer *buffer);

/**
 * Returns a chunk to the buffer.  It can be reused by
 * music_buffer_allocate() then.
 */
void
music_buffer_return(struct music_buffer *buffer, struct music_chunk *chunk);

#endif
