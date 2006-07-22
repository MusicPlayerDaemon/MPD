/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com)
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

#include <stdlib.h>

#ifdef HAVE_FIFO

#include "../conf.h"
#include "../log.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define FIFO_BUFFER_SIZE 65536 /* pipe capacity on Linux >= 2.6.11 */

typedef struct _FifoData {
	char *path;
	int input;
	int output;
	int created;
} FifoData;

static FifoData *newFifoData()
{
	FifoData *ret;

	ret = malloc(sizeof(FifoData));

	ret->path = NULL;
	ret->input = -1;
	ret->output = -1;
	ret->created = 0;
	
	return ret;
}

static void freeFifoData(FifoData *fd)
{
	if (fd->path) free(fd->path);
	free(fd);
}

static int makeFifo(FifoData *fd)
{
	if (mkfifo(fd->path, 0666) < 0) {
		ERROR("Couldn't create FIFO \"%s\": %s\n", fd->path,
		      strerror(errno));
		return -1;
	}

	fd->created = 1;

	return 0;
}

static int checkFifo(FifoData *fd)
{
	struct stat info;

	if (stat(fd->path, &info) < 0) {
		if (errno == ENOENT) {
			/* Path doesn't exist */
			return makeFifo(fd);
		}

		ERROR("Failed to stat FIFO \"%s\": %s\n", fd->path,
		      strerror(errno));
		return -1;
	}

	if (!S_ISFIFO(info.st_mode)) {
		ERROR("\"%s\" already exists, but is not a FIFO\n", fd->path);
		return -1;
	}

	return 0;
}

static int openFifo(FifoData *fd)
{
	if (checkFifo(fd) < 0) return -1;

	fd->input = open(fd->path, O_RDONLY|O_NONBLOCK);
	if (fd->input < 0) {
		ERROR("Could not open FIFO \"%s\" for reading: %s\n", fd->path,
		      strerror(errno));
		return -1;
	}

	fd->output = open(fd->path, O_WRONLY|O_NONBLOCK);
	if (fd->output < 0) {
		ERROR("Could not open FIFO \"%s\" for writing: %s\n", fd->path,
		      strerror(errno));
		return -1;
	}

	return 0;
}

static void removeFifo(FifoData *fd)
{
	DEBUG("Removing FIFO \"%s\"\n", fd->path);
	if (unlink(fd->path) < 0) {
		ERROR("Could not remove FIFO \"%s\": %s\n", fd->path,
		      strerror(errno));
		return;
	}

	fd->created = 0;
}

static void closeFifo(FifoData *fd)
{
	struct stat info;

	if (fd->input >= 0) close(fd->input);
	if (fd->output >= 0) close(fd->output);
	if (fd->created && (stat(fd->path, &info) == 0)) removeFifo(fd);
}

static int fifo_initDriver(AudioOutput *audioOutput, ConfigParam *param)
{
	BlockParam *path = NULL;
	FifoData *fd;

	if (param) path = getBlockParam(param, "path");

	if (!path) {
		ERROR("No \"path\" parameter specified for fifo output "
		      "defined at line %i\n", param->line);
		exit(EXIT_FAILURE);
	}

	if (path->value[0] != '/') {
		ERROR("\"path\" parameter for fifo output is not an absolute "
		      "path at line %i\n", param->line);
		exit(EXIT_FAILURE);
	}

	fd = newFifoData();
	fd->path = strdup(path->value);
	audioOutput->data = fd;

	if (openFifo(fd) < 0) {
		freeFifoData(fd);
		return -1;
	}

	return 0;
}

static void fifo_finishDriver(AudioOutput *audioOutput)
{
	FifoData *fd;

	fd = audioOutput->data;
	closeFifo(fd);
	freeFifoData(fd);
}

static int fifo_openDevice(AudioOutput *audioOutput)
{
	audioOutput->open = 1;
	return 0;
}

static void fifo_dropBufferedAudio(AudioOutput *audioOutput)
{
	FifoData *fd;
	char buf[FIFO_BUFFER_SIZE];
	int bytes = 1;

	fd = audioOutput->data;

	while (bytes > 0) bytes = read(fd->input, buf, FIFO_BUFFER_SIZE);
	if (bytes < 0 && errno != EAGAIN) {
		WARNING("Flush of FIFO \"%s\" failed: %s\n", fd->path,
		        strerror(errno));
	}
}

static void fifo_closeDevice(AudioOutput *audioOutput)
{
	audioOutput->open = 0;
}

static int fifo_playAudio(AudioOutput *audioOutput, char *playChunk, int size)
{
	FifoData *fd;
	int bytes;
	int offset = 0;

	fd = audioOutput->data;

	while (1) {
		bytes = write(fd->output, &playChunk[offset], size);
		if (bytes < 0 && errno == EAGAIN) {
			fifo_dropBufferedAudio(audioOutput);
		} else if (bytes < 0) {
			ERROR("Closing FIFO output \"%s\" due to write error: "
			      "%s\n", fd->path, strerror(errno));
			fifo_closeDevice(audioOutput);
			return -1;
		} else if (bytes < size) {
			size -= bytes;
			offset += bytes;
		} else {
			break;
		}
	}

	return 0;
}

AudioOutputPlugin fifoPlugin = {
	"fifo",
	NULL, /* testDefaultDeviceFunc */
	fifo_initDriver,
	fifo_finishDriver,
	fifo_openDevice,
	fifo_playAudio,
	fifo_dropBufferedAudio,
	fifo_closeDevice,
	NULL, /* sendMetadataFunc */
};

#else /* HAVE_FIFO */

DISABLED_AUDIO_OUTPUT_PLUGIN(fifoPlugin)

#endif /* HAVE_FIFO */
