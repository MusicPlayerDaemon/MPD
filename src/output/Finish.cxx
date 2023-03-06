// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Filtered.hxx"
#include "Interface.hxx"
#include "mixer/Control.hxx"
#include "filter/Prepared.hxx"

FilteredAudioOutput::~FilteredAudioOutput()
{
	if (mixer != nullptr)
		mixer_free(mixer);
}
