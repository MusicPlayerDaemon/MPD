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

#include "inputStream.h"

#include "inputStream_file.h"
#include "inputStream_http.h"

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

void initInputStream(void)
{
	inputStream_initFile();
	inputStream_initHttp();
}

int openInputStream(InputStream * inStream, char *url)
{
	inStream->offset = 0;
	inStream->size = 0;
	inStream->error = 0;
	inStream->mime = NULL;
	inStream->seekable = 0;
	inStream->metaName = NULL;
	inStream->metaTitle = NULL;

	if (inputStream_fileOpen(inStream, url) == 0)
		return 0;
	if (inputStream_httpOpen(inStream, url) == 0)
		return 0;

	return -1;
}

int seekInputStream(InputStream * inStream, long offset, int whence)
{
	return inStream->seekFunc(inStream, offset, whence);
}

size_t readFromInputStream(InputStream * inStream, void *ptr, size_t size,
			   size_t nmemb)
{
	return inStream->readFunc(inStream, ptr, size, nmemb);
}

int closeInputStream(InputStream * inStream)
{
	if (inStream->mime)
		free(inStream->mime);
	if (inStream->metaName)
		free(inStream->metaName);
	if (inStream->metaTitle)
		free(inStream->metaTitle);

	return inStream->closeFunc(inStream);
}

int inputStreamAtEOF(InputStream * inStream)
{
	return inStream->atEOFFunc(inStream);
}

int bufferInputStream(InputStream * inStream)
{
	return inStream->bufferFunc(inStream);
}
