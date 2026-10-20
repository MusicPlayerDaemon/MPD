// SPDX-License-Identifier: GPL-2.0-or-later
// Stubs so that read_mixer links when the PipeWire sink mixer is
// built into libmixer_plugins but the PipeWire output is not.

#include "output/plugins/PipeWireOutputPlugin.hxx"

void
pipewire_output_set_pw_sink_mixer(PipeWireOutput &, PwSinkMixer &) noexcept
{
}

void
pipewire_output_clear_pw_sink_mixer(PipeWireOutput &, PwSinkMixer &) noexcept
{
}

void
pipewire_output_set_volume(PipeWireOutput &, float)
{
}
