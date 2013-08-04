/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
 * This filter copies audio data between channels. Useful for
 * upmixing mono/stereo audio to surround speaker configurations.
 *
 * Its configuration consists of a "filter" section with a single
 * "routes" entry, formatted as: \\
 * routes "0>1, 1>0, 2>2, 3>3, 3>4" \\
 * where each pair of numbers signifies a set of channels.
 * Each source>dest pair leads to the data from channel #source
 * being copied to channel #dest in the output.
 *
 * Example: \\
 * routes "0>0, 1>1, 0>2, 1>3"\\
 * upmixes stereo audio to a 4-speaker system, copying the front-left
 * (0) to front left (0) and rear left (2), copying front-right (1) to
 * front-right (1) and rear-right (3).
 *
 * If multiple sources are copied to the same destination channel, only
 * one of them takes effect.
 */

#include "config.h"
#include "conf.h"
#include "ConfigQuark.hxx"
#include "AudioFormat.hxx"
#include "CheckAudioFormat.hxx"
#include "FilterPlugin.hxx"
#include "FilterInternal.hxx"
#include "FilterRegistry.hxx"
#include "pcm/PcmBuffer.hxx"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

class RouteFilter final : public Filter {
	/**
	 * The minimum number of channels we need for output
	 * to be able to perform all the copies the user has specified
	 */
	unsigned char min_output_channels;

	/**
	 * The minimum number of input channels we need to
	 * copy all the data the user has requested. If fewer
	 * than this many are supplied by the input, undefined
	 * copy operations are given zeroed sources in stead.
	 */
	unsigned char min_input_channels;

	/**
	 * The set of copy operations to perform on each sample
	 * The index is an output channel to use, the value is
	 * a corresponding input channel from which to take the
	 * data. A -1 means "no source"
	 */
	signed char* sources;

	/**
	 * The actual input format of our signal, once opened
	 */
	AudioFormat input_format;

	/**
	 * The decided upon output format, once opened
	 */
	AudioFormat output_format;

	/**
	 * The size, in bytes, of each multichannel frame in the
	 * input buffer
	 */
	size_t input_frame_size;

	/**
	 * The size, in bytes, of each multichannel frame in the
	 * output buffer
	 */
	size_t output_frame_size;

	/**
	 * The output buffer used last time around, can be reused if the size doesn't differ.
	 */
	PcmBuffer output_buffer;

public:
	RouteFilter():sources(nullptr) {}
	~RouteFilter() {
		g_free(sources);
	}

	/**
	 * Parse the "routes" section, a string on the form
	 *  a>b, c>d, e>f, ...
	 * where a... are non-unique, non-negative integers
	 * and input channel a gets copied to output channel b, etc.
	 * @param param the configuration block to read
	 * @param filter a route_filter whose min_channels and sources[] to set
	 * @return true on success, false on error
	 */
	bool Configure(const config_param &param, GError **error_r);

	virtual AudioFormat Open(AudioFormat &af, GError **error_r) override;
	virtual void Close();
	virtual const void *FilterPCM(const void *src, size_t src_size,
				      size_t *dest_size_r, GError **error_r);
};

bool
RouteFilter::Configure(const config_param &param, GError **error_r) {

	/* TODO:
	 * With a more clever way of marking "don't copy to output N",
	 * This could easily be merged into a single loop with some
	 * dynamic g_realloc() instead of one count run and one g_malloc().
	 */

	gchar **tokens;
	int number_of_copies;

	// A cowardly default, just passthrough stereo
	const char *const routes = param.GetBlockValue("routes", "0>0, 1>1");

	min_input_channels = 0;
	min_output_channels = 0;

	tokens = g_strsplit(routes, ",", 255);
	number_of_copies = g_strv_length(tokens);

	// Start by figuring out a few basic things about the routing set
	for (int c=0; c<number_of_copies; ++c) {

		// String and int representations of the source/destination
		gchar **sd;
		int source, dest;

		// Squeeze whitespace
		g_strstrip(tokens[c]);

		// Split the a>b string into source and destination
		sd = g_strsplit(tokens[c], ">", 2);
		if (g_strv_length(sd) != 2) {
			g_set_error(error_r, config_quark(), 1,
				"Invalid copy around %d in routes spec: %s",
				param.line, tokens[c]);
			g_strfreev(sd);
			g_strfreev(tokens);
			return false;
		}

		source = strtol(sd[0], NULL, 10);
		dest = strtol(sd[1], NULL, 10);

		// Keep track of the highest channel numbers seen
		// as either in- or outputs
		if (source >= min_input_channels)
			min_input_channels = source + 1;
		if (dest >= min_output_channels)
			min_output_channels = dest + 1;

		g_strfreev(sd);
	}

	if (!audio_valid_channel_count(min_output_channels)) {
		g_strfreev(tokens);
		g_set_error(error_r, audio_format_quark(), 0,
			    "Invalid number of output channels requested: %d",
			    min_output_channels);
		return false;
	}

	// Allocate a map of "copy nothing to me"
	sources = (signed char *)
		g_malloc(min_output_channels * sizeof(signed char));

	for (int i=0; i<min_output_channels; ++i)
		sources[i] = -1;

	// Run through the spec again, and save the
	// actual mapping output <- input
	for (int c=0; c<number_of_copies; ++c) {

		// String and int representations of the source/destination
		gchar **sd;
		int source, dest;

		// Split the a>b string into source and destination
		sd = g_strsplit(tokens[c], ">", 2);
		if (g_strv_length(sd) != 2) {
			g_set_error(error_r, config_quark(), 1,
				"Invalid copy around %d in routes spec: %s",
				param.line, tokens[c]);
			g_strfreev(sd);
			g_strfreev(tokens);
			return false;
		}

		source = strtol(sd[0], NULL, 10);
		dest = strtol(sd[1], NULL, 10);

		sources[dest] = source;

		g_strfreev(sd);
	}

	g_strfreev(tokens);

	return true;
}

static Filter *
route_filter_init(const config_param &param, GError **error_r)
{
	RouteFilter *filter = new RouteFilter();
	if (!filter->Configure(param, error_r)) {
		delete filter;
		return nullptr;
	}

	return filter;
}

AudioFormat
RouteFilter::Open(AudioFormat &audio_format, gcc_unused GError **error_r)
{
	// Copy the input format for later reference
	input_format = audio_format;
	input_frame_size = input_format.GetFrameSize();

	// Decide on an output format which has enough channels,
	// and is otherwise identical
	output_format = audio_format;
	output_format.channels = min_output_channels;

	// Precalculate this simple value, to speed up allocation later
	output_frame_size = output_format.GetFrameSize();

	return output_format;
}

void
RouteFilter::Close()
{
	output_buffer.Clear();
}

const void *
RouteFilter::FilterPCM(const void *src, size_t src_size,
		       size_t *dest_size_r, gcc_unused GError **error_r)
{
	size_t number_of_frames = src_size / input_frame_size;

	const size_t bytes_per_frame_per_channel = input_format.GetSampleSize();

	// A moving pointer that always refers to channel 0 in the input, at the currently handled frame
	const uint8_t *base_source = (const uint8_t *)src;

	// A moving pointer that always refers to the currently filled channel of the currently handled frame, in the output
	uint8_t *chan_destination;

	// Grow our reusable buffer, if needed, and set the moving pointer
	*dest_size_r = number_of_frames * output_frame_size;
	chan_destination = (uint8_t *)output_buffer.Get(*dest_size_r);

	// Perform our copy operations, with N input channels and M output channels
	for (unsigned int s=0; s<number_of_frames; ++s) {

		// Need to perform one copy per output channel
		for (unsigned int c=0; c<min_output_channels; ++c) {
			if (sources[c] == -1 ||
			    (unsigned)sources[c] >= input_format.channels) {
				// No source for this destination output,
				// give it zeroes as input
				memset(chan_destination,
					0x00,
					bytes_per_frame_per_channel);
			} else {
				// Get the data from channel sources[c]
				// and copy it to the output
				const uint8_t *data = base_source +
					(sources[c] * bytes_per_frame_per_channel);
				memcpy(chan_destination,
					  data,
					  bytes_per_frame_per_channel);
			}
			// Move on to the next output channel
			chan_destination += bytes_per_frame_per_channel;
		}


		// Go on to the next N input samples
		base_source += input_frame_size;
	}

	// Here it is, ladies and gentlemen! Rerouted data!
	return (void *) output_buffer.buffer;
}

const struct filter_plugin route_filter_plugin = {
	"route",
	route_filter_init,
};
