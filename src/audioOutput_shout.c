/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#include "audioOutput.h"
#include "conf.h"
#include "log.h"
#include "sig_handlers.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

#include <shout/shout.h>
#include <vorbis/vorbisenc.h>

static int shoutInitCount = 0;

typedef struct _ShoutData {
	shout_t * shoutConn;
} ShoutData;

static ShoutData * newShoutData() {
	ShoutData * ret = malloc(sizeof(ShoutData));

	ret->shoutConn = shout_new();

	return ret;
}

static void freeShoutData(ShoutData * sd) {
	if(sd->shoutConn) shout_free(sd->shoutConn);

	free(sd);
}

static int shout_initDriver(AudioOutput * audioOutput) {
	ShoutData * sd;
	char * test;
	int port;
	char * host;
	char * mount;
	char * passwd;
	char * user;
	char * name;

	if(!getConf()[CONF_SHOUT_HOST]) {
		return -1;
	}

	sd = newShoutData();

	if(!getConf()[CONF_SHOUT_MOUNT]) {
		ERROR("shout host defined but not shout mount point\n");
		exit(EXIT_FAILURE);
	}

	if(!getConf()[CONF_SHOUT_PORT]) {
		ERROR("shout host defined but not shout port\n");
		exit(EXIT_FAILURE);
	}

	if(!getConf()[CONF_SHOUT_PASSWD]) {
		ERROR("shout host defined but not shout password\n");
		exit(EXIT_FAILURE);
	}

	if(!getConf()[CONF_SHOUT_NAME]) {
		ERROR("shout host defined but not shout name\n");
		exit(EXIT_FAILURE);
	}

	if(!getConf()[CONF_SHOUT_USER]) {
		ERROR("shout host defined but not shout user\n");
		exit(EXIT_FAILURE);
	}

	host = getConf()[CONF_SHOUT_HOST];
	passwd = getConf()[CONF_SHOUT_PASSWD];
	user = getConf()[CONF_SHOUT_USER];
	mount = getConf()[CONF_SHOUT_MOUNT];
	name = getConf()[CONF_SHOUT_NAME];

	port = strtol(getConf()[CONF_SHOUT_PORT], &test, 10);

	if(*test != '\0' || port <= 0) {
		ERROR("shout port \"%s\" is not a positive integer\n", 
				getConf()[CONF_SHOUT_PORT]);
		exit(EXIT_FAILURE);
	}

	if(shout_set_host(sd->shoutConn, host) !=  SHOUTERR_SUCCESS ||
		shout_set_port(sd->shoutConn, port) != SHOUTERR_SUCCESS ||
		shout_set_password(sd->shoutConn, passwd) != SHOUTERR_SUCCESS ||
		shout_set_mount(sd->shoutConn, mount) != SHOUTERR_SUCCESS ||
		shout_set_name(sd->shoutConn, name) != SHOUTERR_SUCCESS ||
		shout_set_user(sd->shoutConn, user) != SHOUTERR_SUCCESS ||
		shout_set_format(sd->shoutConn, SHOUT_FORMAT_VORBIS) 
			!= SHOUTERR_SUCCESS ||
		shout_set_protocol(sd->shoutConn, SHOUT_PROTOCOL_HTTP)
			!= SHOUTERR_SUCCESS)
	{
		ERROR("error configuring shout: %s\n", 
				shout_get_error(sd->shoutConn));
		exit(EXIT_FAILURE);
	}

	audioOutput->data = sd;

	if(shoutInitCount == 0) shout_init();

	shoutInitCount++;

	return 0;
}

static void shout_finishDriver(AudioOutput * audioOutput) {
	ShoutData * sd = (ShoutData *)audioOutput->data;

	freeShoutData(sd);

	shoutInitCount--;

	if(shoutInitCount == 0) shout_shutdown();
}

static void shout_closeDevice(AudioOutput * audioOutput) {
	ShoutData * sd = (ShoutData *) audioOutput->data;

	if(shout_close(sd->shoutConn) != SHOUTERR_SUCCESS)
	{
		ERROR("problem closing connection to shout server: %s\n",
				shout_get_error(sd->shoutConn));
	}

	audioOutput->open = 0;
}

static int shout_openDevice(AudioOutput * audioOutput,
		AudioFormat * audioFormat) 
{
	ShoutData * sd = (ShoutData *)audioOutput->data;

	if(shout_open(sd->shoutConn) != SHOUTERR_SUCCESS)
	{
		ERROR("problem opening connection to shout server: %s\n",
				shout_get_error(sd->shoutConn));
		return -1;
	}

	audioOutput->open = 1;

	return 0;
}


static int shout_play(AudioOutput * audioOutput, char * playChunk, int play) {
	return 0;
}

AudioOutputPlugin shoutPlugin = 
{
	"shout",
	shout_initDriver,
	shout_finishDriver,
	shout_openDevice,
	shout_play,
	shout_closeDevice
};
