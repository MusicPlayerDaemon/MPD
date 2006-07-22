/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com)
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

#include <math.h>
#include <limits.h>

#include "conf.h"
#include "normalize.h"
#include "playlist.h"

/* silence level, apparently this is Wrong (tm) */
#define SILENCE_LEVEL (SHRT_MAX * 0.01)
/* not sure what this is :) */
#define MID           (SHRT_MAX * 0.25)

#define MUL_MIN 0.1
#define MUL_MAX 5.0
#define NSAMPLES 128
#define MIN_SAMPLE_SIZE 32000

#define clamp(a,min,max) (((a)>(max))?(max):(((a)<(min))?(min):(a)))

void normalizeData(char *buffer, int bufferSize, AudioFormat *format)
{
	static float multiplier = 1.0;
	static int current_id = 0;
	float average = 0.0;
	static int old_song = 0;
	int new_song = 0;
	int total_length = 0;
	int temp = 0;
	int i = 0;
	float root_mean_square = 0.0; /* the rms of the data */
	mpd_sint16 *data = (mpd_sint16 *) buffer; /* the audio data */
	int length = bufferSize / 2; /* the number of samples */
	static struct {
		float avg; /* average sample 'level' */
		int len;   /* sample size (used to weigh sample) */
	} mem[NSAMPLES];

	/* operate only on 16 bit, 2 channel audio */
	if (format->bits != 16 && format->channels != 2) return;

	/* calculate the root mean square of the data */
	for (i = 0; i < length; i++)
		root_mean_square += (float)(data[i] * data[i]);

	root_mean_square = sqrt(root_mean_square / (float)length);

	/* reset the multiplier if the song has changed */
	if (old_song != (new_song = getPlaylistCurrentSong())) {
		old_song = new_song;
		/* re-zero 'mem' */
		for (i = 0; i < NSAMPLES; i++) {
			mem[i].avg = 0.0;
			mem[i].len = 0;
		}
		current_id = 0;
	}

	/* and now do magic tricks */
	for (i = 0; i < NSAMPLES; i++) {
		average += mem[i].avg * (float)mem[i].len;
		total_length += mem[i].len;
	}

	if (total_length > MIN_SAMPLE_SIZE) {
		average /= (float) total_length;
		if (average >= SILENCE_LEVEL) {
			multiplier = MID / average;
			/* clamp multiplier */
			multiplier = clamp(multiplier, MUL_MIN, MUL_MAX);
		}
	}

	/* scale and clamp the samples */
	for (i = 0; i < length; i++) {
		temp = data[i] * multiplier;
		data[i] = clamp(temp, SHRT_MIN, SHRT_MAX);
	}

	mem[current_id].len = bufferSize / 2;
	mem[current_id].avg = multiplier * root_mean_square;
	current_id = (current_id + 1) % NSAMPLES; /* increment current_id */
}
