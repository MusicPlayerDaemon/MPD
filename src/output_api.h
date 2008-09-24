/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef OUTPUT_API_H
#define OUTPUT_API_H

#include "../config.h"
#include "pcm_utils.h"
#include "audio_format.h"
#include "tag.h"
#include "conf.h"
#include "log.h"
#include "notify.h"
#include "os_compat.h"

#define DISABLED_AUDIO_OUTPUT_PLUGIN(plugin) const struct audio_output_plugin plugin;

struct audio_output;

struct audio_output_plugin {
	const char *name;

	int (*test_default_device)(void);

	int (*init)(struct audio_output *ao,
		    const struct audio_format *audio_format,
		    ConfigParam *param);

	void (*finish)(struct audio_output *ao);

	int (*open)(struct audio_output *ao,
		    struct audio_format *audio_format);

	int (*play)(struct audio_output *ao,
		    const char *playChunk, size_t size);

	void (*cancel)(struct audio_output *ao);

	void (*close)(struct audio_output *ao);

	void (*send_tag)(struct audio_output *audioOutput,
			 const struct tag *tag);
};

enum audio_output_command {
	AO_COMMAND_NONE = 0,
	AO_COMMAND_OPEN,
	AO_COMMAND_CLOSE,
	AO_COMMAND_PLAY,
	AO_COMMAND_CANCEL,
	AO_COMMAND_SEND_TAG,
	AO_COMMAND_KILL
};

struct audio_output {
	int open;
	const char *name;

	const struct audio_output_plugin *plugin;

	struct audio_format inAudioFormat;
	struct audio_format outAudioFormat;
	struct audio_format reqAudioFormat;
	ConvState convState;
	char *convBuffer;
	size_t convBufferLen;

	pthread_t thread;
	struct notify notify;
	enum audio_output_command command;
	union {
		struct {
			const char *data;
			size_t size;
		} play;

		const struct tag *tag;
	} args;
	int result;

	void *data;
};

extern struct notify audio_output_client_notify;

const char *audio_output_get_name(const struct audio_output *ao);

void audio_output_closed(struct audio_output *ao);

#endif
