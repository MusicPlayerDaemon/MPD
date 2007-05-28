/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

#include "../config.h"
#include "state_file.h"
#include "conf.h"
#include "gcc.h"
#include "log.h"
#include "audio.h"
#include "playlist.h"
#include "utils.h"
#include "volume.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

static struct _sf_cb {
	void (*reader)(FILE *);
	void (*writer)(FILE *);
} sf_callbacks [] = {
	{ read_sw_volume_state, save_sw_volume_state },
	{ readAudioDevicesState, saveAudioDevicesState },
	{ readPlaylistState, savePlaylistState },
};

static const char *sfpath;

static void get_state_file_path(void)
{
	ConfigParam *param;
	if (sfpath)
		return;
	param = parseConfigFilePath(CONF_STATE_FILE, 0);
	if (param)
		sfpath = (const char *)param->value;
}

void write_state_file(void)
{
	int i;
	FILE *fp;

	if (!sfpath)
		return;
	while (!(fp = fopen(sfpath, "w")) && errno == EINTR);

	if (mpd_unlikely(!fp)) {
		ERROR("problems opening state file \"%s\" for writing: %s\n",
		      sfpath, strerror(errno));
		return;
	}

	for (i = 0; i < ARRAY_SIZE(sf_callbacks); i++)
		sf_callbacks[i].writer(fp);

	while(fclose(fp) && errno == EINTR) /* nothing */;
}

void read_state_file(void)
{
	struct stat st;
	int i;
	FILE *fp;

	get_state_file_path();
	if (!sfpath)
		return;
	if (stat(sfpath, &st) < 0) {
		DEBUG("failed to stat state file: %s\n", sfpath);
		return;
	}
	if (!S_ISREG(st.st_mode))
		FATAL("state file \"%s\" is not a regular file\n", sfpath);

	while (!(fp = fopen(sfpath, "r")) && errno == EINTR);
	if (mpd_unlikely(!fp)) {
		FATAL("problems opening state file \"%s\" for reading: %s\n",
		      sfpath, strerror(errno));
	}
	for (i = 0; i < ARRAY_SIZE(sf_callbacks); i++) {
		sf_callbacks[i].reader(fp);
		rewind(fp);
	}

	while(fclose(fp) && errno == EINTR) /* nothing */;
}

void mpd_noreturn state_file_fatal(void)
{
	FATAL("error parsing state file \"%s\"\n", sfpath);
	exit(EXIT_FAILURE);
}

