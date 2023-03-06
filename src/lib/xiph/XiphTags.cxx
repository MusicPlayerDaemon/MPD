// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
