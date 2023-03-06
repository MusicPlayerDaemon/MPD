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
		const auto &ao = outputs.Get(i);

		r.Fmt(FMT_STRING("outputid: {}\n"
				 "outputname: {}\n"
				 "plugin: {}\n"
				 "outputenabled: {}\n"),
		      i,
		      ao.GetName(), ao.GetPluginName(),
		      (unsigned)ao.IsEnabled());

		for (const auto &[attribute, value] : ao.GetAttributes())
			r.Fmt(FMT_STRING("attribute: {}={}\n"),
			      attribute, value);
	}
}
