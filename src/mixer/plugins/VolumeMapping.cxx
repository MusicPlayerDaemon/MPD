// SPDX-License-Identifier: ISC
// Copyright (c) 2010 Clemens Ladisch <clemens@ladisch.de>

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

#include "VolumeMapping.hxx"

#include <math.h>

#define MAX_LINEAR_DB_SCALE	24

static constexpr bool use_linear_dB_scale(long dBmin, long dBmax) noexcept
{
	return dBmax - dBmin <= MAX_LINEAR_DB_SCALE * 100;
}

[[gnu::const]]
static long lrint_dir(double x, int dir) noexcept
{
	if (dir > 0)
		return lrint(ceil(x));
	else if (dir < 0)
		return lrint(floor(x));
	else
		return lrint(x);
}

enum ctl_dir { PLAYBACK, CAPTURE };

static constexpr int (* const get_dB_range[2])(snd_mixer_elem_t *, long *, long *) = {
	snd_mixer_selem_get_playback_dB_range,
	snd_mixer_selem_get_capture_dB_range,
};

static constexpr int (* const get_raw_range[2])(snd_mixer_elem_t *, long *, long *) = {
	snd_mixer_selem_get_playback_volume_range,
	snd_mixer_selem_get_capture_volume_range,
};

static constexpr int (* const get_dB[2])(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long *) = {
	snd_mixer_selem_get_playback_dB,
	snd_mixer_selem_get_capture_dB,
};

static constexpr int (* const get_raw[2])(snd_mixer_elem_t *, snd_mixer_selem_channel_id_t, long *) = {
	snd_mixer_selem_get_playback_volume,
	snd_mixer_selem_get_capture_volume,
};

static constexpr int (* const set_dB[2])(snd_mixer_elem_t *, long, int) = {
	snd_mixer_selem_set_playback_dB_all,
	snd_mixer_selem_set_capture_dB_all,
};

static constexpr int (* const set_raw[2])(snd_mixer_elem_t *, long) = {
	snd_mixer_selem_set_playback_volume_all,
	snd_mixer_selem_set_capture_volume_all,
};

[[gnu::pure]]
static double
get_normalized_volume(snd_mixer_elem_t *elem,
		      snd_mixer_selem_channel_id_t channel,
		      enum ctl_dir ctl_dir) noexcept
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

	normalized = pow(10, (value - max) / 6000.0);
	if (min != SND_CTL_TLV_DB_GAIN_MUTE) {
		min_norm = pow(10, (min - max) / 6000.0);
		normalized = (normalized - min_norm) / (1 - min_norm);
	}

	return normalized;
}

static int
set_normalized_volume(snd_mixer_elem_t *elem,
		      double volume,
		      int dir,
		      enum ctl_dir ctl_dir) noexcept
{
	long min, max, value;
	double min_norm;
	int err;

	err = get_dB_range[ctl_dir](elem, &min, &max);
	if (err < 0 || min >= max) {
		err = get_raw_range[ctl_dir](elem, &min, &max);
		if (err < 0)
			return err;

		/* two special cases to avoid rounding errors at 0% and
		   100% */
		if (volume <= 0)
			return set_raw[ctl_dir](elem, min);
		else if (volume >= 1)
			return set_raw[ctl_dir](elem, max);

		value = lrint_dir(volume * (max - min), dir) + min;
		return set_raw[ctl_dir](elem, value);
	}

	/* two special cases to avoid rounding errors at 0% and
	   100% */
	if (volume <= 0)
		return set_dB[ctl_dir](elem, min, dir);
	else if (volume >= 1)
		return set_dB[ctl_dir](elem, max, dir);

	if (use_linear_dB_scale(min, max)) {
		value = lrint_dir(volume * (max - min), dir) + min;
		return set_dB[ctl_dir](elem, value, dir);
	}

	if (min != SND_CTL_TLV_DB_GAIN_MUTE) {
		min_norm = pow(10, (min - max) / 6000.0);
		volume = volume * (1 - min_norm) + min_norm;
	}
	value = lrint_dir(6000.0 * log10(volume), dir) + max;
	return set_dB[ctl_dir](elem, value, dir);
}

double
get_normalized_playback_volume(snd_mixer_elem_t *elem,
			       snd_mixer_selem_channel_id_t channel) noexcept
{
	return get_normalized_volume(elem, channel, PLAYBACK);
}

double
get_normalized_capture_volume(snd_mixer_elem_t *elem,
			      snd_mixer_selem_channel_id_t channel) noexcept
{
	return get_normalized_volume(elem, channel, CAPTURE);
}

int
set_normalized_playback_volume(snd_mixer_elem_t *elem,
			       double volume,
			       int dir) noexcept
{
	return set_normalized_volume(elem, volume, dir, PLAYBACK);
}

int
set_normalized_capture_volume(snd_mixer_elem_t *elem,
			      double volume,
			      int dir) noexcept
{
	return set_normalized_volume(elem, volume, dir, CAPTURE);
}
