/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
 * (c)2004 replayGain code by AliasMrJones
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

#include "replayGain.h"

#include "log.h"
#include "conf.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
 
/* Added 4/14/2004 by AliasMrJones */
static int replayGainState = REPLAYGAIN_OFF;

static float replayGainPreamp = 1.0;

void initReplayGainState() {
	if(!getConf()[CONF_REPLAYGAIN]) return;

	if(strcmp(getConf()[CONF_REPLAYGAIN],"track")==0) {
		replayGainState = REPLAYGAIN_TRACK;
	}
	else if(strcmp(getConf()[CONF_REPLAYGAIN],"album")==0) {
		replayGainState = REPLAYGAIN_ALBUM;
	}
	else {
		ERROR("replaygain value \"%s\" is invalid\n",
				getConf()[CONF_REPLAYGAIN]);
		exit(EXIT_FAILURE);
	}

	if(getConf()[CONF_REPLAYGAIN_PREAMP]) {
		char * test;
		float f = strtod(getConf()[CONF_REPLAYGAIN_PREAMP], &test);

		if(*test != '\0') {
			ERROR("Replaygain preamp \"%s\" is not a number\n",
					getConf()[CONF_REPLAYGAIN_PREAMP]);
			exit(EXIT_FAILURE);
		}

		if(f < -15 || f > 15) {
			ERROR("Replaygain preamp \"%s\" is not between -15 and"
					"15\n", 
					getConf()[CONF_REPLAYGAIN_PREAMP]);
			exit(EXIT_FAILURE);
		}

		replayGainPreamp = pow(10, f/20.0);
	}
}

int getReplayGainState() {
	return replayGainState;
}

float computeReplayGainScale(float gain, float peak) {
	float scale;

	if(gain == 0.0) return(1);
	scale = pow(10.0, gain/20.0);
	scale*= replayGainPreamp;
	if(scale > 15.0) scale = 15.0;

	if (scale * peak > 1.0) {
		scale = 1.0 / peak;
	}
	return(scale);
}

void doReplayGain(char * buffer, int bufferSize, AudioFormat * format, 
		float scale) 
{
	mpd_sint16 * buffer16 = (mpd_sint16 *)buffer;
	mpd_sint8 * buffer8 = (mpd_sint8 *)buffer;
	mpd_sint32 temp32;

	if(scale == 1.0) return;
	switch(format->bits) {
		case 16:
			while(bufferSize > 0){
				temp32 = *buffer16;
				temp32 *= scale;
				*buffer16 = temp32>32767 ? 32767 : 
					(temp32<-32768 ? -32768 : temp32);
				buffer16++;
				bufferSize-=2;
			}
			break;
		case 8:
			while(bufferSize>0){
				temp32 = *buffer8;
				temp32 *= scale;
				*buffer8 = temp32>127 ? 127 : 
					(temp32<-128 ? -128 : temp32);
				buffer8++;
				bufferSize--;
			}
			break;
		default:
			ERROR("%i bits not supported by doReplaygain!\n", format->bits);
	}
}
/* End of added code */
