#ifndef REPLAYGAIN_H
#define REPLAYGAIN_H

#include "audio.h"

#define REPLAYGAIN_OFF		0
#define REPLAYGAIN_TRACK	1
#define REPLAYGAIN_ALBUM	2

void initReplayGainState();

int getReplayGainState();

float computeReplayGainScale(float gain, float peak);

void doReplayGain(char * buffer, int bufferSize, AudioFormat * format, 
		float scale);

#endif
