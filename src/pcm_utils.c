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


#include "pcm_utils.h"

#include "mpd_types.h"
#include "log.h"

#include <string.h>
#include <math.h>

void pcm_changeBufferEndianness(char * buffer, int bufferSize, int bits) {
	char temp;

	switch(bits) {
	case 16:
		while(bufferSize) {
			temp = *buffer;
			*buffer = *(buffer+1);
			*(buffer+1) = temp;
			bufferSize-=2;
		}
		break;
	}
}

void pcm_volumeChange(char * buffer, int bufferSize, AudioFormat * format,
		int volume)
{
	mpd_sint32 temp32;
	mpd_sint8 * buffer8 = (mpd_sint8 *)buffer;
	mpd_sint16 * buffer16 = (mpd_sint16 *)buffer;

	if(volume>=1000) return;
	
	if(volume<=0) {
		memset(buffer,0,bufferSize);
		return;
	}

	switch(format->bits) {
	case 16:
		while(bufferSize>0) {
			temp32 = *buffer16;
			temp32*= volume;
			temp32/=1000;
			*buffer16 = temp32>32767 ? 32767 : 
					(temp32<-32768 ? -32768 : temp32);
			buffer16++;
			bufferSize-=2;
		}
		break;
	case 8:
		while(bufferSize>0) {
			temp32 = *buffer8;
			temp32*= volume;
			temp32/=1000;
			*buffer8 = temp32>127 ? 127 : 
					(temp32<-128 ? -128 : temp32);
			buffer8++;
			bufferSize--;
		}
		break;
	default:
		ERROR("%i bits not supported by pcm_volumeChange!\n",
				format->bits);
		exit(EXIT_FAILURE);
	}
}

void pcm_add(char * buffer1, char * buffer2, size_t bufferSize1, 
		size_t bufferSize2, int vol1, int vol2, AudioFormat * format)
{
	mpd_sint32 temp32;
	mpd_sint8 * buffer8_1 = (mpd_sint8 *)buffer1;
	mpd_sint8 * buffer8_2 = (mpd_sint8 *)buffer2;
	mpd_sint16 * buffer16_1 = (mpd_sint16 *)buffer1;
	mpd_sint16 * buffer16_2 = (mpd_sint16 *)buffer2;

	switch(format->bits) {
	case 16:
		while(bufferSize1>0 && bufferSize2>0) {
			temp32 = (vol1*(*buffer16_1)+vol2*(*buffer16_2))/1000;
			*buffer16_1 = temp32>32767 ? 32767 : 
					(temp32<-32768 ? -32768 : temp32);
			buffer16_1++;
			buffer16_2++;
			bufferSize1-=2;
			bufferSize2-=2;
		}
		if(bufferSize2>0) memcpy(buffer16_1,buffer16_2,bufferSize2);
		break;
	case 8:
		while(bufferSize1>0 && bufferSize2>0) {
			temp32 = (vol1*(*buffer8_1)+vol2*(*buffer8_2))/1000;
			*buffer8_1 = temp32>127 ? 127 : 
					(temp32<-128 ? -128 : temp32);
			buffer8_1++;
			buffer8_2++;
			bufferSize1--;
			bufferSize2--;
		}
		if(bufferSize2>0) memcpy(buffer8_1,buffer8_2,bufferSize2);
		break;
	default:
		ERROR("%i bits not supported by pcm_add!\n",format->bits);
		exit(EXIT_FAILURE);
	}
}

void pcm_mix(char * buffer1, char * buffer2, size_t bufferSize1, 
		size_t bufferSize2, AudioFormat * format, float portion1)
{
	int vol1;
	float s = sin(M_PI_2*portion1);
	s*=s;
	
	vol1 = s*1000+0.5;
	vol1 = vol1>1000 ? 1000 : ( vol1<0 ? 0 : vol1 );

	pcm_add(buffer1,buffer2,bufferSize1,bufferSize2,vol1,1000-vol1,format);
}
