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

typedef struct _ShoutData {
} ShoutData;

static ShoutData * newShoutData() {
	ShoutData * ret = malloc(sizeof(ShoutData));

	return ret;
}

static void freeShoutData(ShoutData * sd) {
	free(sd);
}

static void shout_initDriver(AudioOutput * audioOutput) {
	ShoutData * sd  = newShoutData();

	audioOutput->data = sd;
}

static void shout_finishDriver(AudioOutput * audioOutput) {
	ShoutData * sd = (ShoutData *)audioOutput->data;

	freeShoutData(sd);
}

static void shout_closeDevice(AudioOutput * audioOutput) {
	/*ShoutData * sd = (ShoutData *) audioOutput->data;*/

	audioOutput->open = 0;
}

static int shout_openDevice(AudioOutput * audioOutput,
		AudioFormat * audioFormat) 
{
	/*ShoutData * sd = (ShoutData *)audioOutput->data;*/

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
