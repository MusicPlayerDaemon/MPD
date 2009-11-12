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

/*
 * WARNING!  This plugin suffers from major shortcomings in the
 * libfluidsynth API, which render it practically unusable.  For a
 * discussion, see the post on the fluidsynth mailing list:
 *
 * http://www.mail-archive.com/fluid-dev@nongnu.org/msg01099.html
 *
 */

#include "config.h"
#include "decoder_api.h"
#include "timer.h"
#include "conf.h"

#include <glib.h>

#include <fluidsynth.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "fluidsynth"

/**
 * Convert a fluidsynth log level to a GLib log level.
 */
static GLogLevelFlags
fluidsynth_level_to_glib(enum fluid_log_level level)
{
	switch (level) {
	case FLUID_PANIC:
	case FLUID_ERR:
		return G_LOG_LEVEL_CRITICAL;

	case FLUID_WARN:
		return G_LOG_LEVEL_WARNING;

	case FLUID_INFO:
		return G_LOG_LEVEL_INFO;

	case FLUID_DBG:
	case LAST_LOG_LEVEL:
		return G_LOG_LEVEL_DEBUG;
	}

	/* invalid fluidsynth log level */
	return G_LOG_LEVEL_MESSAGE;
}

/**
 * The fluidsynth logging callback.  It forwards messages to the GLib
 * logging library.
 */
static void
fluidsynth_mpd_log_function(int level, char *message, G_GNUC_UNUSED void *data)
{
	g_log(G_LOG_DOMAIN, fluidsynth_level_to_glib(level), "%s", message);
}

static bool
fluidsynth_init(G_GNUC_UNUSED const struct config_param *param)
{
	fluid_set_log_function(LAST_LOG_LEVEL,
			       fluidsynth_mpd_log_function, NULL);

	return true;
}

static void
fluidsynth_file_decode(struct decoder *decoder, const char *path_fs)
{
	static const struct audio_format audio_format = {
		.sample_rate = 48000,
		.bits = 16,
		.channels = 2,
	};
	char setting_sample_rate[] = "synth.sample-rate";
	/*
	char setting_verbose[] = "synth.verbose";
	char setting_yes[] = "yes";
	*/
	const char *soundfont_path;
	fluid_settings_t *settings;
	fluid_synth_t *synth;
	fluid_player_t *player;
	char *path_dup;
	int ret;
	Timer *timer;
	enum decoder_command cmd;

	soundfont_path =
		config_get_string("soundfont",
				  "/usr/share/sounds/sf2/FluidR3_GM.sf2");

	/* set up fluid settings */

	settings = new_fluid_settings();
	if (settings == NULL)
		return;

	fluid_settings_setnum(settings, setting_sample_rate, 48000);

	/*
	fluid_settings_setstr(settings, setting_verbose, setting_yes);
	*/

	/* create the fluid synth */

	synth = new_fluid_synth(settings);
	if (synth == NULL) {
		delete_fluid_settings(settings);
		return;
	}

	ret = fluid_synth_sfload(synth, soundfont_path, true);
	if (ret < 0) {
		g_warning("fluid_synth_sfload() failed");
		delete_fluid_synth(synth);
		delete_fluid_settings(settings);
		return;
	}

	/* create the fluid player */

	player = new_fluid_player(synth);
	if (player == NULL) {
		delete_fluid_synth(synth);
		delete_fluid_settings(settings);
		return;
	}

	/* temporarily duplicate the path_fs string, because
	   fluidsynth wants a writable string */
	path_dup = g_strdup(path_fs);
	ret = fluid_player_add(player, path_dup);
	g_free(path_dup);
	if (ret != 0) {
		g_warning("fluid_player_add() failed");
		delete_fluid_player(player);
		delete_fluid_synth(synth);
		delete_fluid_settings(settings);
		return;
	}

	/* start the player */

	ret = fluid_player_play(player);
	if (ret != 0) {
		g_warning("fluid_player_play() failed");
		delete_fluid_player(player);
		delete_fluid_synth(synth);
		delete_fluid_settings(settings);
		return;
	}

	/* set up a timer for synchronization; fluidsynth always
	   decodes in real time, which forces us to synchronize */
	/* XXX is there any way to switch off real-time decoding? */

	timer = timer_new(&audio_format);
	timer_start(timer);

	/* initialization complete - announce the audio format to the
	   MPD core */

	decoder_initialized(decoder, &audio_format, false, -1);

	do {
		int16_t buffer[2048];
		const unsigned max_frames = G_N_ELEMENTS(buffer) / 2;

		/* synchronize with the fluid player */

		timer_add(timer, sizeof(buffer));
		timer_sync(timer);

		/* read samples from fluidsynth and send them to the
		   MPD core */

		ret = fluid_synth_write_s16(synth, max_frames,
					    buffer, 0, 2,
					    buffer, 1, 2);
		/* XXX how do we see whether the player is done?  We
		   can't access the private attribute
		   player->status */
		if (ret != 0)
			break;

		cmd = decoder_data(decoder, NULL, buffer, sizeof(buffer),
				   0, 0, NULL);
	} while (cmd == DECODE_COMMAND_NONE);

	/* clean up */

	timer_free(timer);

	fluid_player_stop(player);
	fluid_player_join(player);

	delete_fluid_player(player);
	delete_fluid_synth(synth);
	delete_fluid_settings(settings);
}

static struct tag *
fluidsynth_tag_dup(const char *file)
{
	struct tag *tag = tag_new();

	/* to be implemented */
	(void)file;

	return tag;
}

static const char *const fluidsynth_suffixes[] = {
	"mid",
	NULL
};

const struct decoder_plugin fluidsynth_decoder_plugin = {
	.name = "fluidsynth",
	.init = fluidsynth_init,
	.file_decode = fluidsynth_file_decode,
	.tag_dup = fluidsynth_tag_dup,
	.suffixes = fluidsynth_suffixes,
};
