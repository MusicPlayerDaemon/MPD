// SPDX-License-Identifier: ISC

#pragma once

#include <alsa/asoundlib.h>

[[gnu::pure]]
double
get_normalized_playback_volume(snd_mixer_elem_t *elem,
			       snd_mixer_selem_channel_id_t channel) noexcept;

[[gnu::pure]]
double
get_normalized_capture_volume(snd_mixer_elem_t *elem,
			      snd_mixer_selem_channel_id_t channel) noexcept;

int
set_normalized_playback_volume(snd_mixer_elem_t *elem,
			       double volume,
			       int dir) noexcept;

int
set_normalized_capture_volume(snd_mixer_elem_t *elem,
			      double volume,
			      int dir) noexcept;

/**
 * Calculate the normalized volume which attenuates (or amplifies)
 * the signal by the given linear factor relative to the given
 * normalized volume, for a control with the given dB range (in
 * 0.01 dB units).
 *
 * @param volume the normalized volume (0..1) which corresponds to
 * scale=1
 * @param scale the linear scale factor (non-negative)
 * @return the new normalized volume (0..1)
 */
[[gnu::const]]
double
scale_normalized_volume_dB(double volume, double scale,
			   long dBmin, long dBmax) noexcept;

/**
 * Like scale_normalized_volume_dB(), but determine the dB range of
 * the given playback control.  If the control does not have dB
 * information, then the normalized volume is assumed to be linear.
 */
[[gnu::pure]]
double
scale_normalized_playback_volume(snd_mixer_elem_t *elem,
				 double volume, double scale) noexcept;
