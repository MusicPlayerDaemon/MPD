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

#include "inputStream.h"

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

int openInputStreamFromFile(InputStream * inStream, char * filename) {
	inStream->fp = fopen(filename,"r");
	if(!inStream->fp) {
		inStream->error = errno;
		return -1;
	}

	inStream->offset = 0;

	fseek(inStream->fp,0,SEEK_END);
	inStream->size = ftell(inStream->fp);
	fseek(inStream->fp,0,SEEK_SET);

	return 0;
}

int seekInputStream(InputStream * inStream, long offset, int whence) {
	if(fseek(inStream->fp,offset,whence)==0) {
		inStream->offset = ftell(inStream->fp);
	}
	else {
		inStream->error = errno;
		return -1;
	}

	return 0;
}

size_t readFromInputStream(InputStream * inStream, void * ptr, size_t size, 
		size_t nmemb)
{
	size_t readSize;

	readSize = fread(ptr,size,nmemb,inStream->fp);

	if(readSize>0) inStream->offset+=readSize;

	return readSize;
}

int closeInputStream(InputStream * inStream) {
	if(fclose(inStream->fp)<0) {
		inStream->error = errno;
	}
	else return -1;

	return 0;
}

int inputStreamAtEOF(InputStream * inStream) {
	return feof(inStream->fp);
}
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
