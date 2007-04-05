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

#include "inputStream_file.h"

#include "log.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#define _XOPEN_SOURCE 600
#include <fcntl.h>

void inputStream_initFile(void)
{
}

int inputStream_fileOpen(InputStream * inStream, char *filename)
{
	FILE *fp;

	fp = fopen(filename, "r");
	if (!fp) {
		inStream->error = errno;
		return -1;
	}

	inStream->seekable = 1;

	fseek(fp, 0, SEEK_END);
	inStream->size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

#ifdef POSIX_FADV_SEQUENTIAL
	posix_fadvise(fileno(fp), (off_t)0, inStream->size, POSIX_FADV_SEQUENTIAL);
#endif

	inStream->data = fp;
	inStream->seekFunc = inputStream_fileSeek;
	inStream->closeFunc = inputStream_fileClose;
	inStream->readFunc = inputStream_fileRead;
	inStream->atEOFFunc = inputStream_fileAtEOF;
	inStream->bufferFunc = inputStream_fileBuffer;

	return 0;
}

int inputStream_fileSeek(InputStream * inStream, long offset, int whence)
{
	if (fseek((FILE *) inStream->data, offset, whence) == 0) {
		inStream->offset = ftell((FILE *) inStream->data);
	} else {
		inStream->error = errno;
		return -1;
	}

	return 0;
}

size_t inputStream_fileRead(InputStream * inStream, void *ptr, size_t size,
			    size_t nmemb)
{
	size_t readSize;

	readSize = fread(ptr, size, nmemb, (FILE *) inStream->data);
	if (readSize <= 0 && ferror((FILE *) inStream->data)) {
		inStream->error = errno;
		DEBUG("inputStream_fileRead: error reading: %s\n",
		      strerror(inStream->error));
	}

	inStream->offset = ftell((FILE *) inStream->data);

	return readSize;
}

int inputStream_fileClose(InputStream * inStream)
{
	if (fclose((FILE *) inStream->data) < 0) {
		inStream->error = errno;
		return -1;
	}

	return 0;
}

int inputStream_fileAtEOF(InputStream * inStream)
{
	if (feof((FILE *) inStream->data))
		return 1;

	if (ferror((FILE *) inStream->data) && inStream->error != EINTR) {
		return 1;
	}

	return 0;
}

int inputStream_fileBuffer(InputStream * inStream)
{
	return 0;
}
