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
#include <assert.h>

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


/* outFormat bits must be 16 and channels must be 2! */
void pcm_convertAudioFormat(AudioFormat * inFormat, char * inBuffer, size_t
                inSize, AudioFormat * outFormat, char * outBuffer)
{
	static char * bitConvBuffer = NULL;
	static int bitConvBufferLength = 0;
	static char * channelConvBuffer = NULL;
	static int channelConvBufferLength = 0;
	char * dataChannelConv;
	int dataChannelLen;
	char * dataBitConv;
	int dataBitLen;

	assert(outFormat->bits==16);
	assert(outFormat->channels==2);

	/* converts */
	switch(inFormat->bits) {
	case 8:
		dataBitLen = inSize << 1;
		if(dataBitLen > bitConvBufferLength) {
			bitConvBuffer = realloc(bitConvBuffer, dataBitLen);
			bitConvBufferLength = dataBitLen;
		}
		dataBitConv = bitConvBuffer;
		{
			mpd_sint8 * in = (mpd_sint8 *)inBuffer;
			mpd_sint16 * out = (mpd_sint16 *)dataBitConv;
			int i;
			for(i=0; i<inSize; i++) {
				*out++ = (*in++) << 8;
			}
		}
		break;
	case 16:
		dataBitConv = inBuffer;
		dataBitLen = inSize;
		break;
	case 24:
		/* put dithering code from mp3_decode here */
	default:
		ERROR("only 8 or 16 bits are supported for conversion!\n");
		exit(EXIT_FAILURE);
	}

	/* converts only between 16 bit audio between mono and stereo */
	switch(inFormat->channels) {
	case 1:
		dataChannelLen = (dataBitLen >> 1) << 2;
		if(dataChannelLen > channelConvBufferLength) {
			channelConvBuffer = realloc(channelConvBuffer,
					dataChannelLen);
			channelConvBufferLength = dataChannelLen;
		}
		dataChannelConv = channelConvBuffer;
		{
			mpd_sint16 * in = (mpd_sint16 *)dataBitConv;
			mpd_sint16 * out = (mpd_sint16 *)dataChannelConv;
			int i, inSamples = dataBitLen >> 1;
			for(i=0;i<inSamples;i++) {
				*out++ = *in;
				*out++ = *in++;
			}
		}
		break;
	case 2:
		dataChannelConv = dataBitConv;
		dataChannelLen = dataBitLen;
		break;
	default:
		ERROR("only 1 or 2 channels are supported for conversion!\n");
		exit(EXIT_FAILURE);
	}

	if(inFormat->sampleRate == outFormat->sampleRate) {
		memcpy(outBuffer,dataChannelConv,dataChannelLen);
	}
	else {
		/* only works if outFormat is 16-bit stereo! */
		/* resampling code blatantly ripped from XMMS */
		const int shift = sizeof(mpd_sint16);
		mpd_sint32 i, in_samples, out_samples, x, delta;
		mpd_sint16 * inptr = (mpd_sint16 *)dataChannelConv;
		mpd_sint16 * outptr = (mpd_sint16 *)outBuffer;
		mpd_uint32 nlen = (((dataChannelLen >> shift) * 
				(outFormat->sampleRate)) / 
				inFormat->sampleRate);
		nlen <<= shift;
		in_samples = dataChannelLen >> shift;
		out_samples = nlen >> shift;
		delta = (in_samples << 12) / out_samples;
		for(x = 0, i = 0; i < out_samples; i++) {
			int x1, frac;
			x1 = (x >> 12) << 12;
			frac = x - x1;
			*outptr++ = 
				((inptr[(x1 >> 12) << 1] * 
				((1<<12) - frac) +
				inptr[((x1 >> 12) + 1) << 1 ] *
				frac) >> 12);
			*outptr++ =
				((inptr[((x1 >> 12) << 1) + 1] *
				((1<<12) - frac) +
				inptr[(((x1 >> 12) + 1) << 1) + 1] *
				frac) >> 12);
			x += delta;
		}
	}

	return;
}

size_t pcm_sizeOfOutputBufferForAudioFormatConversion(AudioFormat * inFormat,
		char * inBuffer, size_t inSize, AudioFormat * outFormat)
{
	const int shift = sizeof(mpd_sint16);
	size_t outSize = inSize;

	switch(inFormat->bits) {
	case 8:
		outSize = outSize << 1;
		break;
	case 16:
		break;
	default:
		ERROR("only 8 or 16 bits are supported for conversion!\n");
		exit(EXIT_FAILURE);
	}

	switch(inFormat->channels) {
	case 1:
		outSize = (outSize >> 1) << 2;
		break;
	case 2:
		break;
	default:
		ERROR("only 1 or 2 channels are supported for conversion!\n");
		exit(EXIT_FAILURE);
	}
	
	outSize = (((outSize >> shift) * (mpd_uint32)(outFormat->sampleRate)) /
				inFormat->sampleRate);

	outSize <<= shift;

	return outSize;
}

/* vim:set shiftwidth=8 tabstop=8 expandtab: */
