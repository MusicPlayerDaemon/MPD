/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

/*
 * Protocol specific code for the audio output library.
 *
 */

#include "Print.hxx"
#include "MultipleOutputs.hxx"
#include "client/Response.hxx"

void
printAudioDevices(Response &r, const MultipleOutputs &outputs)
{
	for (unsigned i = 0, n = outputs.Size(); i != n; ++i) {
		const auto &ao = outputs.Get(i);

		r.Format("outputid: %u\n"
			 "outputname: %s\n"
			 "plugin: %s\n"
			 "outputenabled: %i\n",
			 i,
			 ao.GetName(), ao.GetPluginName(),
			 ao.IsEnabled());

		for (const auto &a : ao.GetAttributes())
			r.Format("attribute: %s=%s\n",
				 a.first.c_str(), a.second.c_str());
	}
}
