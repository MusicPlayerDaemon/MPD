/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
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

#include "crossfade.h"
#include "audio.h"
#include "pcm_utils.h"

unsigned cross_fade_calc(float duration, float total_time,
			 const struct audio_format *af,
			 unsigned max_chunks)
{
	unsigned int chunks;

	if (duration <= 0 || duration >= total_time ||
	    !isCurrentAudioFormat(af))
		return 0;

	assert(duration > 0);
	assert(af->bits > 0);
	assert(af->channels > 0);
	assert(af->sampleRate > 0);

	chunks = audio_format_time_to_size(af) / CHUNK_SIZE;
	chunks = (chunks * duration + 0.5);

	if (chunks > max_chunks)
		chunks = max_chunks;

	return chunks;
}

void cross_fade_apply(ob_chunk * a, const ob_chunk * b,
		      const struct audio_format *format,
		      unsigned int current_chunk, unsigned int num_chunks)
{
	assert(current_chunk <= num_chunks);

	pcm_mix(a->data,
		b->data,
		a->chunkSize,
		b->chunkSize,
		format,
		((float)current_chunk) / num_chunks);
	if (b->chunkSize > a->chunkSize)
		a->chunkSize = b->chunkSize;
}
