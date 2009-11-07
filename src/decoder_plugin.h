/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#ifndef MPD_DECODER_PLUGIN_H
#define MPD_DECODER_PLUGIN_H

#include <stdbool.h>
#include <stddef.h>

struct config_param;
struct input_stream;
struct tag;

/**
 * Opaque handle which the decoder plugin passes to the functions in
 * this header.
 */
struct decoder;

struct decoder_plugin {
	const char *name;

	/**
	 * Initialize the decoder plugin.  Optional method.
	 *
	 * @param param a configuration block for this plugin, or NULL
	 * if none is configured
	 * @return true if the plugin was initialized successfully,
	 * false if the plugin is not available
	 */
	bool (*init)(const struct config_param *param);

	/**
	 * Deinitialize a decoder plugin which was initialized
	 * successfully.  Optional method.
	 */
	void (*finish)(void);

	/**
	 * Decode a stream (data read from an #input_stream object).
	 *
	 * Either implement this method or file_decode().  If
	 * possible, it is recommended to implement this method,
	 * because it is more versatile.
	 */
	void (*stream_decode)(struct decoder *decoder,
			      struct input_stream *is);

	/**
	 * Decode a local file.
	 *
	 * Either implement this method or stream_decode().
	 */
	void (*file_decode)(struct decoder *decoder, const char *path_fs);

	/**
	 * Read the tags of a local file.
	 *
	 * @return NULL if the operation has failed
	 */
	struct tag *(*tag_dup)(const char *path_fs);

	/**
	 * @brief Return a "virtual" filename for subtracks in
	 * container formats like flac
	 * @param const char* pathname full pathname for the file on fs
	 * @param const unsigned int tnum track number
	 *
	 * @return NULL if there are no multiple files
	 * a filename for every single track according to tnum (param 2)
	 * do not include full pathname here, just the "virtual" file
	 */
	char* (*container_scan)(const char *path_fs, const unsigned int tnum);

	/* last element in these arrays must always be a NULL: */
	const char *const*suffixes;
	const char *const*mime_types;
};

/**
 * Initialize a decoder plugin.
 *
 * @param param a configuration block for this plugin, or NULL if none
 * is configured
 * @return true if the plugin was initialized successfully, false if
 * the plugin is not available
 */
static inline bool
decoder_plugin_init(const struct decoder_plugin *plugin,
		    const struct config_param *param)
{
	return plugin->init != NULL
		? plugin->init(param)
		: true;
}

/**
 * Deinitialize a decoder plugin which was initialized successfully.
 */
static inline void
decoder_plugin_finish(const struct decoder_plugin *plugin)
{
	if (plugin->finish != NULL)
		plugin->finish();
}

/**
 * Decode a stream.
 */
static inline void
decoder_plugin_stream_decode(const struct decoder_plugin *plugin,
			     struct decoder *decoder, struct input_stream *is)
{
	plugin->stream_decode(decoder, is);
}

/**
 * Decode a file.
 */
static inline void
decoder_plugin_file_decode(const struct decoder_plugin *plugin,
			   struct decoder *decoder, const char *path_fs)
{
	plugin->file_decode(decoder, path_fs);
}

/**
 * Read the tag of a file.
 */
static inline struct tag *
decoder_plugin_tag_dup(const struct decoder_plugin *plugin,
		       const char *path_fs)
{
	return plugin->tag_dup(path_fs);
}

/**
 * return "virtual" tracks in a container
 */
static inline char *
decoder_plugin_container_scan(	const struct decoder_plugin *plugin,
				const char* pathname,
				const unsigned int tnum)
{
	return plugin->container_scan(pathname, tnum);
}

/**
 * Does the plugin announce the specified file name suffix?
 */
bool
decoder_plugin_supports_suffix(const struct decoder_plugin *plugin,
			       const char *suffix);

/**
 * Does the plugin announce the specified MIME type?
 */
bool
decoder_plugin_supports_mime_type(const struct decoder_plugin *plugin,
				  const char *mime_type);

#endif
