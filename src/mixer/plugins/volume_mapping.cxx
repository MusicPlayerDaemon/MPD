/*
 * Copyright (c) 2010 Clemens Ladisch <clemens@ladisch.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * The functions in this file map the value ranges of ALSA mixer controls onto
 * the interval 0..1.
 *
 * The mapping is designed so that the position in the interval is proportional
 * to the volume as a human ear would perceive it (i.e., the position is the
 * cubic root of the linear sample multiplication factor).  For controls with
 * a small range (24 dB or less), the mapping is linear in the dB values so
 * that each step has the same size visually.  Only for controls without dB
 * information, a linear mapping of the hardware volume register values is used
 * (this is the same algorithm as used in the old alsamixer).
 *
 * When setting the volume, 'dir' is the rounding direction:
 * -1/0/1 = down/nearest/up.
 */

#define _ISOC99_SOURCE /* lrint() */
#define _GNU_SOURCE /* exp10() */
#include "aconfig.h"
#include <math.h>
#include <stdbool.h>
#include "volume_mapping.h"

#ifdef __UCLIBC__
/* 10^x = 10^(log e^x) = (e^x)^log10 = e^(x * log 10) */
#define exp10(x) (exp((x) * log(10)))
#endif /* __UCLIBC__ */

#define MAX_LINEAR_DB_SCALE	24

static inline bool use_linear_dB_scale(long dBmin, long dBmax)
{
	return dBmax - dBmin <= MAX_LINEAR_DB_SCALE * 100;
}

static long lrint_dir(double x, int dir)
{
	if (dir > 0)
		return lrint(ceil(x));
	else if (dir < 0)
		return lrint(floor(x));
	else
		return lrint(x);
}

enum ctl_dir { PLAYBACK, CAPTURE };

static int (* const get_dB_range[2])(snd_mixer_elem_t *, long *, long *) = {
	snd_mixer_selem_get_playback_dB_range,
	snd_mixer_selem_get_capture_dB_range,
};
static int (* const get_raw_range[2])(snd_mixer_elem_t *, long *, long *) = {
	snd_mixer_selem_get_playback_volume_range,
	snd_mixer_selem_get_capture_volume_range,
};
static int (* const get_dB[2])(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long *) = {
	snd_mixer_selem_get_playback_dB,
	snd_mixer_selem_get_capture_dB,
};
static int (* const get_raw[2])(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long *) = {
	snd_mixer_selem_get_playback_volume,
	snd_mixer_selem_get_capture_volume,
};
static int (* const set_dB[2])(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long, int) = {
	snd_mixer_selem_set_playback_dB,
	snd_mixer_selem_set_capture_dB,
};
static int (* const set_raw[2])(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long) = {
	snd_mixer_selem_set_playback_volume,
	snd_mixer_selem_set_capture_volume,
};

static double get_normalized_volume(snd_mixer_elem_t *elem,
				    snd_mixer_selem_channel_id_t channel,
				    enum ctl_dir ctl_dir)
{
	long min, max, value;
	double normalized, min_norm;
	int err;

	err = get_dB_range[ctl_dir](elem, &min, &max);
	if (err < 0 || min >= max) {
		err = get_raw_range[ctl_dir](elem, &min, &max);
		if (err < 0 || min == max)
			return 0;

		err = get_raw[ctl_dir](elem, channel, &value);
		if (err < 0)
			return 0;

		return (value - min) / (double)(max - min);
	}

	err = get_dB[ctl_dir](elem, channel, &value);
	if (err < 0)
		return 0;

	if (use_linear_dB_scale(min, max))
		return (value - min) / (double)(max - min);

	normalized = exp10((value - max) / 6000.0);
	if (min != SND_CTL_TLV_DB_GAIN_MUTE) {
		min_norm = exp10((min - max) / 6000.0);
		normalized = (normalized - min_norm) / (1 - min_norm);
	}

	return normalized;
}

static int set_normalized_volume(snd_mixer_elem_t *elem,
				 snd_mixer_selem_channel_id_t channel,
				 double volume,
				 int dir,
				 enum ctl_dir ctl_dir)
{
	long min, max, value;
	double min_norm;
	int err;

	err = get_dB_range[ctl_dir](elem, &min, &max);
	if (err < 0 || min >= max) {
		err = get_raw_range[ctl_dir](elem, &min, &max);
		if (err < 0)
			return err;

		value = lrint_dir(volume * (max - min), dir) + min;
		return set_raw[ctl_dir](elem, channel, value);
	}

	if (use_linear_dB_scale(min, max)) {
		value = lrint_dir(volume * (max - min), dir) + min;
		return set_dB[ctl_dir](elem, channel, value, dir);
	}

	if (min != SND_CTL_TLV_DB_GAIN_MUTE) {
		min_norm = exp10((min - max) / 6000.0);
		volume = volume * (1 - min_norm) + min_norm;
	}
	value = lrint_dir(6000.0 * log10(volume), dir) + max;
	return set_dB[ctl_dir](elem, channel, value, dir);
}

double get_normalized_playback_volume(snd_mixer_elem_t *elem,
				      snd_mixer_selem_channel_id_t channel)
{
	return get_normalized_volume(elem, channel, PLAYBACK);
}

double get_normalized_capture_volume(snd_mixer_elem_t *elem,
				     snd_mixer_selem_channel_id_t channel)
{
	return get_normalized_volume(elem, channel, CAPTURE);
}

int set_normalized_playback_volume(snd_mixer_elem_t *elem,
				   snd_mixer_selem_channel_id_t channel,
				   double volume,
				   int dir)
{
	return set_normalized_volume(elem, channel, volume, dir, PLAYBACK);
}

int set_normalized_capture_volume(snd_mixer_elem_t *elem,
				  snd_mixer_selem_channel_id_t channel,
				  double volume,
				  int dir)
{
	return set_normalized_volume(elem, channel, volume, dir, CAPTURE);
}
