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

int initInputStreamFromFile(InputStream * inStream, char * filename) {
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

int seekInputStream(InputStream * inStream, long offset) {
	if(offset<0) {
		inStream->error = EINVAL;
		return -1;
	}

	if(fseek(inStream->fp,offset,SEEK_SET)==0) {
		inStream->offset = offset;
	}
	else {
		inStream->error = errno;
		return -1;
	}

	return 0;
}

size_t fillBufferFromInputStream(InputStream * inStream, char * buffer, 
		size_t buflen) 
{
	size_t readSize;

	readSize = fread(buffer,1,buflen,inStream->fp);

	if(readSize>0) inStream->offset+=readSize;

	return readSize;
}

int finishInputStream(InputStream * inStream) {
	if(fclose(inStream->fp)<0) {
		inStream->error = errno;
	}
	else return -1;

	return 0;
}
