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

#include <string.h>
#include <ctype.h>

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
