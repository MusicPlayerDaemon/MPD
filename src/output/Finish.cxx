/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "Internal.hxx"
#include "OutputPlugin.hxx"
#include "mixer/MixerControl.hxx"
#include "filter/FilterInternal.hxx"

#include <assert.h>

void
ao_base_finish(AudioOutput *ao)
{
	assert(!ao->open);
	assert(!ao->fail_timer.IsDefined());
	assert(!ao->thread.IsDefined());

	if (ao->mixer != nullptr)
		mixer_free(ao->mixer);

	delete ao->replay_gain_filter;
	delete ao->other_replay_gain_filter;
	delete ao->filter;
}

void
audio_output_free(AudioOutput *ao)
{
	assert(!ao->open);
	assert(!ao->fail_timer.IsDefined());
	assert(!ao->thread.IsDefined());

	ao_plugin_finish(ao);
}
