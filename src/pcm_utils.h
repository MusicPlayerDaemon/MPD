#ifndef PCM_UTILS_H
#define PMC_UTILS_H

#include "audio.h"

#include <stdlib.h>

void pcm_changeBufferEndianness(char * buffer, int bufferSize, int bits);

void pcm_volumeChange(char * buffer, int bufferSize, AudioFormat * format,
		int volume);

void pcm_mix(char * buffer1, char * buffer2, size_t bufferSize1, 
		size_t bufferSize2, AudioFormat * format, float portion1);

#endif
