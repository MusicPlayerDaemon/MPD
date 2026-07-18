// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Protocol specific code for the audio output library.
 *
 */

#include "Print.hxx"
#include "Control.hxx"
#include "MultipleOutputs.hxx"
#include "AllOutputs.hxx"
#include "client/Response.hxx"

#include <fmt/format.h>

using std::string_view_literals::operator""sv;

void
printAudioDevices(Response &r, const MultipleOutputs &outputs)
{
	const auto &all_outputs = outputs.GetAllOutputs();
	for (unsigned i = 0, n = all_outputs.Size(); i != n; ++i) {
		const auto &ao = all_outputs.Get(i);

		r.Fmt("outputid: {}\n"
		       "outputname: {}\n"
		       "plugin: {}\n"
		       "outputenabled: {}\n",
		      i,
		      ao.GetName(), ao.GetPluginName(),
		      outputs.Owns(ao) && ao.IsEnabled() ? "1"sv : "0"sv);

		if (!outputs.Owns(ao))
			/* omit attributes for outputs assigned to a
			   different partition */
			continue;

		for (const auto &[attribute, value] : ao.GetAttributes())
			r.Fmt("attribute: {}={}\n",
			      attribute, value);
	}
}
