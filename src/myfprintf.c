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
#include "path.h"
#include "log.h"
#include "conf.h"

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
char * myfprintf_outFilename;
char * myfprintf_errFilename;

void blockingWrite(int fd, char * string, int len) {
	int ret;

	while(len) {
		ret = write(fd,string,len);
		if(ret==0) return;
		if(ret<0) {
			switch(errno) {
			case EAGAIN:
			case EINTR:
				continue;
			default:
				return;
			}
		}
		len-= ret;
		string+= ret;
	}
}

void myfprintfStdLogMode(FILE * out, FILE * err) {
	myfprintf_stdLogMode = 1;
	myfprintf_out = out;
	myfprintf_err = err;
        myfprintf_outFilename = getConfigParamValue(CONF_LOG_FILE);
        myfprintf_errFilename = getConfigParamValue(CONF_ERROR_FILE);
}

void myfprintf(FILE * fp, char * format, ... ) {
	char buffer[BUFFER_LENGTH+1];
	va_list arglist;
	int fd = fileno(fp);

	memset(buffer,0,BUFFER_LENGTH+1);

	va_start(arglist,format);
	if(fd==1 || fd==2) {
		if(myfprintf_stdLogMode) {
			time_t t = time(NULL);
			if(fd==1) fp = myfprintf_out;
			else fp = myfprintf_err;
			strftime(buffer,14,"%b %e %R",localtime(&t));
			blockingWrite(fd,buffer,strlen(buffer));
			blockingWrite(fd," : ",3);
		}
		vsnprintf(buffer,BUFFER_LENGTH,format,arglist);
		blockingWrite(fd,buffer,strlen(buffer));
	}
	else {
		int len;
		vsnprintf(buffer,BUFFER_LENGTH,format,arglist);
		len = strlen(buffer);
		if(interfacePrintWithFD(fd,buffer,len)<0) {
			blockingWrite(fd,buffer,len);
		}
	}

	va_end(arglist);
}

int myfprintfCloseAndOpenLogFile() {
        if(myfprintf_stdLogMode) {
                while(fclose(myfprintf_out)<0 && errno==EINTR);
                while(fclose(myfprintf_err)<0 && errno==EINTR);
                while((myfprintf_out = fopen(myfprintf_outFilename,"a+"))==NULL
                                && errno==EINTR);
                if(!myfprintf_out) {
                        ERROR("error re-opening log file: %s\n",
                                myfprintf_out);
                        return -1;
                }
                while((myfprintf_err = fopen(myfprintf_errFilename,"a+"))==NULL
                                && errno==EINTR);
                if(!myfprintf_out) {
                        ERROR("error re-opening log file: %s\n",
                                myfprintf_out);
                        return -1;
                }
                while(dup2(fileno(myfprintf_out),1)<0 && errno==EINTR);
                while(dup2(fileno(myfprintf_err),2)<0 && errno==EINTR);
        }

        return 0;
}

void myfprintfCloseLogFile() {
        if(myfprintf_stdLogMode) {
                while(fclose(myfprintf_out)<0 && errno==EINTR);
                while(fclose(myfprintf_err)<0 && errno==EINTR);
	}
}
