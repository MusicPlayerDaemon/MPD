/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

/*
 * Support library for the "idle" command.
 *
 */

#ifndef MPD_IDLE_FLAGS_HXX
#define MPD_IDLE_FLAGS_HXX

/** song database has been updated*/
static constexpr unsigned IDLE_DATABASE = 0x1;

/** a stored playlist has been modified, created, deleted or
    renamed */
static constexpr unsigned IDLE_STORED_PLAYLIST = 0x2;

/** the current playlist has been modified */
static constexpr unsigned IDLE_PLAYLIST = 0x4;

/** the player state has changed: play, stop, pause, seek, ... */
static constexpr unsigned IDLE_PLAYER = 0x8;

/** the volume has been modified */
static constexpr unsigned IDLE_MIXER = 0x10;

/** an audio output device has been enabled or disabled */
static constexpr unsigned IDLE_OUTPUT = 0x20;

/** options have changed: crossfade; random; repeat; ... */
static constexpr unsigned IDLE_OPTIONS = 0x40;

/** a sticker has been modified. */
static constexpr unsigned IDLE_STICKER = 0x80;

/** a database update has started or finished. */
static constexpr unsigned IDLE_UPDATE = 0x100;

/** a client has subscribed or unsubscribed to/from a channel */
static constexpr unsigned IDLE_SUBSCRIPTION = 0x200;

/** a message on the subscribed channel was received */
static constexpr unsigned IDLE_MESSAGE = 0x400;

/** a neighbor was found or lost */
static constexpr unsigned IDLE_NEIGHBOR = 0x800;

/** the mount list has changed */
static constexpr unsigned IDLE_MOUNT = 0x1000;

/** the partition list has changed */
static constexpr unsigned IDLE_PARTITION = 0x2000;

/**
 * Get idle names
 */
[[gnu::const]]
const char*const*
idle_get_names() noexcept;

/**
 * Parse an idle name and return its mask.  Returns 0 if the given
 * name is unknown.
 */
[[gnu::nonnull]] [[gnu::pure]]
unsigned
idle_parse_name(const char *name) noexcept;

#endif
