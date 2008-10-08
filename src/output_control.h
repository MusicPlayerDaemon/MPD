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

#ifndef OUTPUT_CONTROL_H
#define OUTPUT_CONTROL_H

#include "conf.h"

#include <stddef.h>

struct audio_output;
struct audio_output_plugin;
struct audio_format;
struct tag;

int audio_output_init(struct audio_output *, ConfigParam * param);
int audio_output_open(struct audio_output *audioOutput,
		      const struct audio_format *audioFormat);

/**
 * Wakes up the audio output thread.  This is part of a workaround for
 * a deadlock bug, and should be removed as soon as the real cause is
 * fixed.  XXX
 */
void
audio_output_signal(struct audio_output *ao);

void audio_output_play(struct audio_output *audioOutput,
		       const char *playChunk, size_t size);

void audio_output_pause(struct audio_output *audioOutput);

void audio_output_cancel(struct audio_output *audioOutput);
void audio_output_close(struct audio_output *audioOutput);
void audio_output_finish(struct audio_output *audioOutput);
void audio_output_send_tag(struct audio_output *audioOutput,
			   const struct tag *tag);

#endif
