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

#include "../audioOutput.h"

#ifdef HAVE_OSX

#include "../conf.h"
#include "../log.h"

#include <CoreAudio/AudioHardware.h>
#include <stdlib.h>

typedef struct _OsxData {
	AudioDeviceID	deviceID;
} OsxData;

static OsxData * newOsxData() {
	OsxData * ret = malloc(sizeof(OsxData));

	return ret;
}

static int osx_testDefault() {
	int err;
	AudioDeviceID deviceID;
	UInt32 propertySize = sizeof(deviceID);

	err = AudioHardwareGetProperty(
			kAudioHardwarePropertyDefaultOutputDevice,
			&propertySize, &deviceID);
	if(err || deviceID == kAudioDeviceUnknown) {
		WARNING("Not able to get the default OS X device\n");
		return -1;
	}

	return 0;
}

static int osx_initDriver(AudioOutput * audioOutput, ConfigParam * param) {
	OsxData * od  = newOsxData();

	audioOutput->data = od;

	return 0;
}

static void freeOsxData(OsxData * od) {
	free(od);
}

static void osx_finishDriver(AudioOutput * audioOutput) {
	OsxData * od = (OsxData *)audioOutput->data;
	freeOsxData(od);
}

static void osx_dropBufferedAudio(AudioOutput * audioOutput) {
	/* not implemented yet */
}

static void osx_closeDevice(AudioOutput * audioOutput) {
	//OsxData * od = (OsxData *) audioOutput->data;

	audioOutput->open = 0;
}

static int osx_openDevice(AudioOutput * audioOutput) {
	//OsxData * od = (OsxData *)audioOutput->data;

	audioOutput->open = 1;

	return 0;
}


static int osx_play(AudioOutput * audioOutput, char * playChunk, int size) {
	//OsxData * od = (OsxData *)audioOutput->data;

	return 0;
}

AudioOutputPlugin osxPlugin = 
{
	"osx",
	osx_testDefault,
	osx_initDriver,
	osx_finishDriver,
	osx_openDevice,
	osx_play,
	osx_dropBufferedAudio,
	osx_closeDevice,
	NULL /* sendMetadataFunc */
};

#else

#include <stdio.h>

AudioOutputPlugin osxPlugin = 
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

#endif
