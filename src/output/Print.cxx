// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Protocol specific code for the audio output library.
 *
 */

#include "Print.hxx"
#include "MultipleOutputs.hxx"
#include "client/Response.hxx"

#include <fmt/format.h>

void
printAudioDevices(Response &r, const MultipleOutputs &outputs)
{
	for (unsigned i = 0, n = outputs.Size(); i != n; ++i) {
		printAudioDevice(r, outputs, i, true);
	}
}

void
printAudioDevice(Response &r, const MultipleOutputs &outputs, unsigned idx, bool attributes)
{
	const auto &ao = outputs.Get(idx);

	r.Fmt(FMT_STRING("outputid: {}\n"
				"outputname: {}\n"
				"plugin: {}\n"
				"outputenabled: {}\n"),
		idx,
		ao.GetName(), ao.GetPluginName(),
		(unsigned)ao.IsEnabled());
	if (attributes)
		for (const auto &[attribute, value] : ao.GetAttributes())
			r.Fmt(FMT_STRING("attribute: {}={}\n"),
				attribute, value);
}

void
printAudioDeviceList(Response &r, const MultipleOutputs &outputs)
{
	for (unsigned i = 0, n = outputs.Size(); i != n; ++i) {
		printAudioDevice(r, outputs, i, false);
	}
}
