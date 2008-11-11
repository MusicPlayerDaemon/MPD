/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * (c)2004 replayGain code by AliasMrJones
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

#ifndef MPD_REPLAY_GAIN_H
#define MPD_REPLAY_GAIN_H

enum replay_gain_mode {
	REPLAY_GAIN_OFF = -1,
	REPLAY_GAIN_ALBUM,
	REPLAY_GAIN_TRACK,
};

struct audio_format;

extern enum replay_gain_mode replay_gain_mode;

struct replay_gain_tuple {
	float gain;
	float peak;
};

struct replay_gain_info {
	struct replay_gain_tuple tuples[2];

	/* used internally by mpd, to mess with it */
	float scale;
};

struct replay_gain_info *
replay_gain_info_new(void);

void replay_gain_info_free(struct replay_gain_info *info);

void replay_gain_global_init(void);

void
replay_gain_apply(struct replay_gain_info *info, char *buffer, int bufferSize,
		  const struct audio_format *format);

#endif
