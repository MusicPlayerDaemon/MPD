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

#include "buffer2array.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int buffer2array(char * origBuffer, char *** array) {
	int quotes = 0;
	int count = 0;
	int i;
	int curr;
	int * beginArray;
	char * buffer = strdup(origBuffer);
	int bufferLength = strlen(buffer);
	char * markArray = malloc(sizeof(char)*(bufferLength+1));

	for(curr=0;curr<bufferLength;curr++) {
		if(!quotes && (buffer[curr]==' ' || buffer[curr]=='\t') ) {
			markArray[curr] = '0';
		}
		else if(buffer[curr] == '\"') {
			if(curr>0 && buffer[curr-1]!='\\') {
				quotes = quotes?0:1;
				markArray[curr] = '0';
			}
			else {
				markArray[curr] = '1';
			}
		}
		else {
			markArray[curr] = '1';
		}
		if(markArray[curr]=='1') {
			if(curr>0) {
				if(markArray[curr-1]=='0') {
					count++;
				}
			}
			else {
				count++;
			}
		}
	}
	markArray[bufferLength] = '\0';

	if(!count) {
		free(buffer);
		free(markArray);
		return count;
	}

	beginArray = malloc(sizeof(int)*count);
	(*array) = malloc(sizeof(char *)*count);

	count = 0;
	
	for(curr=0;curr<bufferLength;curr++) {
		if(markArray[curr]=='1') {
			if(curr>0) {
				if(markArray[curr-1]=='0') {
					beginArray[count++] = curr;
				}
			}
			else {
				beginArray[count++] = curr;
			}
		}
		else {
			buffer[curr] = '\0';
		}
	}

	for(i=0;i<count;i++) {
		int len = strlen(buffer+beginArray[i])+1;
		int arrayCurr = 0;
		(*array)[i] = malloc(sizeof(char)*len);
		for(curr=beginArray[i];buffer[curr]!='\0';curr++) {
			if(buffer[curr]=='\\') {
				if(buffer[curr+1]!='\0') {
					curr++;
				}
			}
			(*array)[i][arrayCurr++] = buffer[curr];
		}
		(*array)[i][arrayCurr] = '\0';
	}

	free(markArray);
	free(beginArray);
	free(buffer);

	return count;
}

void freeArgArray(char ** array, int argArrayLength) {
	int i;

	if(argArrayLength==0) return;

	for(i=0;i<argArrayLength;i++) {
		free(array[i]);
	}
	free(array);
}
