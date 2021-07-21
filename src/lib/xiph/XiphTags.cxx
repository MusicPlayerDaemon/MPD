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

/* This File contains additional Tags for Xiph-Based Formats like Ogg-Vorbis,
 * Flac and Opus which will be used in addition to the Tags in tag/TagNames.c
 * see https://www.xiph.org/vorbis/doc/v-comment.html for further Info
 */

#include "XiphTags.hxx"

const struct tag_table xiph_tags[] = {
	{ "tracknumber", TAG_TRACK },
	{ "discnumber", TAG_DISC },
	{ "description", TAG_COMMENT },
	{ "movementname", TAG_MOVEMENT },
	{ "movement", TAG_MOVEMENTNUMBER },
	{ nullptr, TAG_NUM_OF_ITEM_TYPES }
};
