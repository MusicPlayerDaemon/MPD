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

#include "inputStream_file.h"

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

int inputStream_fileOpen(InputStream * inStream, char * filename) {
        FILE * fp;

	fp = fopen(filename,"r");
	if(!fp) {
		inStream->error = errno;
		return -1;
	}

	inStream->offset = 0;
        inStream->seekable = 1;
        inStream->mime = NULL;

	fseek(fp,0,SEEK_END);
	inStream->size = ftell(fp);
	fseek(fp,0,SEEK_SET);

        inStream->data = fp;
        inStream->seekFunc = inputStream_fileSeek;
        inStream->closeFunc = inputStream_fileClose;
        inStream->readFunc = inputStream_fileRead;
        inStream->atEOFFunc = inputStream_fileAtEOF;

	return 0;
}

int inputStream_fileSeek(InputStream * inStream, long offset, int whence) {
	if(fseek((FILE *)inStream->data,offset,whence)==0) {
		inStream->offset = ftell((FILE *)inStream->data);
	}
	else {
		inStream->error = errno;
		return -1;
	}

	return 0;
}

size_t inputStream_fileRead(InputStream * inStream, void * ptr, size_t size, 
		size_t nmemb)
{
	size_t readSize;

	readSize = fread(ptr,size,nmemb,(FILE *)inStream->data);

	if(readSize>0) inStream->offset+=readSize;

	return readSize;
}

int inputStream_fileClose(InputStream * inStream) {
	if(fclose((FILE *)inStream->data)<0) {
		inStream->error = errno;
	}
	else return -1;

	return 0;
}

int inputStream_fileAtEOF(InputStream * inStream) {
	return feof((FILE *)inStream->data);
}
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
