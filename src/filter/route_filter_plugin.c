/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "audio_format.h"
#include "audio_check.h"
#include "filter_plugin.h"
#include "filter_internal.h"
#include "filter_registry.h"
#include "pcm_buffer.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>


struct route_filter {

	/**
	 * Inherit (and support cast to/from) filter
	 */
	struct filter base;

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
	struct audio_format input_format;

	/**
	 * The decided upon output format, once opened
	 */
	struct audio_format output_format;

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
	struct pcm_buffer output_buffer;

};

/**
 * Parse the "routes" section, a string on the form
 *  a>b, c>d, e>f, ...
 * where a... are non-unique, non-negative integers
 * and input channel a gets copied to output channel b, etc.
 * @param param the configuration block to read
 * @param filter a route_filter whose min_channels and sources[] to set
 * @return true on success, false on error
 */
static bool
route_filter_parse(const struct config_param *param,
		   struct route_filter *filter,
		   GError **error_r) {

	/* TODO:
	 * With a more clever way of marking "don't copy to output N",
	 * This could easily be merged into a single loop with some
	 * dynamic g_realloc() instead of one count run and one g_malloc().
	 */

	gchar **tokens;
	int number_of_copies;

	// A cowardly default, just passthrough stereo
	const char *routes =
		config_get_block_string(param, "routes", "0>0, 1>1");

	filter->min_input_channels = 0;
	filter->min_output_channels = 0;

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
				param->line, tokens[c]);
			g_strfreev(sd);
			g_strfreev(tokens);
			return false;
		}

		source = strtol(sd[0], NULL, 10);
		dest = strtol(sd[1], NULL, 10);

		// Keep track of the highest channel numbers seen
		// as either in- or outputs
		if (source >= filter->min_input_channels)
			filter->min_input_channels = source + 1;
		if (dest   >= filter->min_output_channels)
			filter->min_output_channels = dest + 1;

		g_strfreev(sd);
	}

	if (!audio_valid_channel_count(filter->min_output_channels)) {
		g_strfreev(tokens);
		g_set_error(error_r, audio_format_quark(), 0,
			    "Invalid number of output channels requested: %d",
			    filter->min_output_channels);
		return false;
	}

	// Allocate a map of "copy nothing to me"
	filter->sources =
		g_malloc(filter->min_output_channels * sizeof(signed char));

	for (int i=0; i<filter->min_output_channels; ++i)
		filter->sources[i] = -1;

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
				param->line, tokens[c]);
			g_strfreev(sd);
			g_strfreev(tokens);
			return false;
		}

		source = strtol(sd[0], NULL, 10);
		dest = strtol(sd[1], NULL, 10);

		filter->sources[dest] = source;

		g_strfreev(sd);
	}

	g_strfreev(tokens);

	return true;
}

static struct filter *
route_filter_init(const struct config_param *param,
		 G_GNUC_UNUSED GError **error_r)
{
	struct route_filter *filter = g_new(struct route_filter, 1);
	filter_init(&filter->base, &route_filter_plugin);

	// Allocate and set the filter->sources[] array
	route_filter_parse(param, filter, error_r);

	return &filter->base;
}

static void
route_filter_finish(struct filter *_filter)
{
	struct route_filter *filter = (struct route_filter *)_filter;

	g_free(filter->sources);
	g_free(filter);
}

static const struct audio_format *
route_filter_open(struct filter *_filter, struct audio_format *audio_format,
		  G_GNUC_UNUSED GError **error_r)
{
	struct route_filter *filter = (struct route_filter *)_filter;

	// Copy the input format for later reference
	filter->input_format = *audio_format;
	filter->input_frame_size =
		audio_format_frame_size(&filter->input_format);

	// Decide on an output format which has enough channels,
	// and is otherwise identical
	filter->output_format = *audio_format;
	filter->output_format.channels = filter->min_output_channels;

	// Precalculate this simple value, to speed up allocation later
	filter->output_frame_size =
		audio_format_frame_size(&filter->output_format);

	// This buffer grows as needed
	pcm_buffer_init(&filter->output_buffer);

	return &filter->output_format;
}

static void
route_filter_close(struct filter *_filter)
{
	struct route_filter *filter = (struct route_filter *)_filter;

	pcm_buffer_deinit(&filter->output_buffer);
}

static const void *
route_filter_filter(struct filter *_filter,
		   const void *src, size_t src_size,
		   size_t *dest_size_r, G_GNUC_UNUSED GError **error_r)
{
	struct route_filter *filter = (struct route_filter *)_filter;

	size_t number_of_frames = src_size / filter->input_frame_size;

	size_t bytes_per_frame_per_channel =
		audio_format_sample_size(&filter->input_format);

	// A moving pointer that always refers to channel 0 in the input, at the currently handled frame
	const uint8_t *base_source = src;

	// A moving pointer that always refers to the currently filled channel of the currently handled frame, in the output
	uint8_t *chan_destination;

	// Grow our reusable buffer, if needed, and set the moving pointer
	*dest_size_r = number_of_frames * filter->output_frame_size;
	chan_destination = pcm_buffer_get(&filter->output_buffer, *dest_size_r);


	// Perform our copy operations, with N input channels and M output channels
	for (unsigned int s=0; s<number_of_frames; ++s) {

		// Need to perform one copy per output channel
		for (unsigned int c=0; c<filter->min_output_channels; ++c) {
			if (filter->sources[c] == -1 ||
			    (unsigned)filter->sources[c] >= filter->input_format.channels) {
				// No source for this destination output,
				// give it zeroes as input
				memset(chan_destination,
					0x00,
					bytes_per_frame_per_channel);
			} else {
				// Get the data from channel sources[c]
				// and copy it to the output
				const uint8_t *data = base_source +
					(filter->sources[c] * bytes_per_frame_per_channel);
				memcpy(chan_destination,
					  data,
					  bytes_per_frame_per_channel);
			}
			// Move on to the next output channel
			chan_destination += bytes_per_frame_per_channel;
		}


		// Go on to the next N input samples
		base_source += filter->input_frame_size;
	}

	// Here it is, ladies and gentlemen! Rerouted data!
	return (void *) filter->output_buffer.buffer;
}

const struct filter_plugin route_filter_plugin = {
	.name = "route",
	.init = route_filter_init,
	.finish = route_filter_finish,
	.open = route_filter_open,
	.close = route_filter_close,
	.filter = route_filter_filter,
};
