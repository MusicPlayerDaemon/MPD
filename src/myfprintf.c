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

#include "myfprintf.h"
#include "interface.h"

#include <stdarg.h>
#include <sys/param.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define BUFFER_LENGTH	MAXPATHLEN+1024

int myfprintf_stdLogMode = 0;
FILE * myfprintf_out;
FILE * myfprintf_err;

void myfprintfStdLogMode(FILE * out, FILE * err) {
	myfprintf_stdLogMode = 1;
	myfprintf_out = out;
	myfprintf_err = err;
}

void myfprintf(FILE * fp, char * format, ... ) {
	va_list arglist;
	int fd = fileno(fp);
	int fcntlret;

	va_start(arglist,format);
	while((fcntlret=fcntl(fd,F_GETFL))==-1 && errno==EINTR);
	if(myfprintf_stdLogMode && (fd==1 || fd==2)) {
		char str[15];
		time_t t = time(NULL);
		if(fd==1) fp = myfprintf_out;
		else fp = myfprintf_err;
		strftime(str,14,"%b %e %R",localtime(&t));
		fprintf(fp,"%s : ",str);
		vfprintf(fp,format,arglist);
	}
	else if(fcntlret & O_NONBLOCK) {
		char buffer[BUFFER_LENGTH+1];
		vsnprintf(buffer,BUFFER_LENGTH,format,arglist);
		if(interfacePrintWithFD(fd,buffer)<0) {
			/* not a fd from a interface */
			vfprintf(fp,format,arglist);
		}
	}
	else vfprintf(fp,format,arglist);
	fflush(fp);

	va_end(arglist);
}
