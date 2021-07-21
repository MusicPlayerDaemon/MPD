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

/** \file
 *
 * This header provides "extern" declarations for all mixer plugins.
 */

#ifndef MPD_MIXER_LIST_HXX
#define MPD_MIXER_LIST_HXX

struct MixerPlugin;

extern const MixerPlugin null_mixer_plugin;
extern const MixerPlugin software_mixer_plugin;
extern const MixerPlugin android_mixer_plugin;
extern const MixerPlugin alsa_mixer_plugin;
extern const MixerPlugin haiku_mixer_plugin;
extern const MixerPlugin oss_mixer_plugin;
extern const MixerPlugin osx_mixer_plugin;
extern const MixerPlugin pulse_mixer_plugin;
extern const MixerPlugin winmm_mixer_plugin;
extern const MixerPlugin wasapi_mixer_plugin;
extern const MixerPlugin sndio_mixer_plugin;

#endif
