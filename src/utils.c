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

#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

char * myFgets(char * buffer, int bufferSize, FILE * fp) {
	char * ret = fgets(buffer,bufferSize,fp);
	if(ret && strlen(buffer)>0 && buffer[strlen(buffer)-1]=='\n') {
		buffer[strlen(buffer)-1] = '\0';
	}
	return ret;
}

char * strDupToUpper(char * str) {
	char * ret = strdup(str);
	int i;
	
	for(i=0;i<strlen(str);i++) ret[i] = toupper((int)ret[i]);
	
	return ret;
}

void stripReturnChar(char * string) {
	while(string && (string = strstr(string,"\n"))) {
		*string = ' ';
	}
}

void my_usleep(long usec) {
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = usec;

	select(0,NULL,NULL,NULL,&tv);
}

int ipv6Supported() {
#ifdef HAVE_IPV6
	int s;
	s = socket(AF_INET6,SOCK_STREAM,0);
	if(s == -1) return 0;
	close(s);
	return 1;
#endif
        return 0;
}

char * appendToString(char * dest, const char * src) {
        int destlen;
        int srclen = strlen(src);
                                                                                
        if(dest == NULL) {
                dest = malloc(srclen+1);
                memset(dest, 0, srclen+1);
                destlen = 0;
        }
        else {
                destlen = strlen(dest);
                dest = realloc(dest, destlen+srclen+1);
        }
                                                                                
        memcpy(dest+destlen, src, srclen);
        dest[destlen+srclen] = '\0';
                                                                                
        return dest;
}
