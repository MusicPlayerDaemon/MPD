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

#include "../audioOutput.h"

#ifdef HAVE_OSX

#include <AudioUnit/AudioUnit.h>
#include <stdlib.h>
#include <pthread.h>

#include "../log.h"

#define BUFFER_SIZE 4096

typedef struct _OsxData {
	AudioUnit au;
	pthread_mutex_t	mutex;
	pthread_cond_t condition;
	char buffer[BUFFER_SIZE];
	int pos;
	int len;
	int go;
	int started;
} OsxData;

static OsxData * newOsxData() {
	OsxData * ret = malloc(sizeof(OsxData));

	pthread_mutex_init(&ret->mutex, NULL);
	pthread_cond_init(&ret->condition, NULL);

	ret->pos = 0;
	ret->len = 0;
	ret->go = 0;
	ret->started = 0;

	return ret;
}

static int osx_testDefault() {
	/*AudioUnit au;
	ComponentDescription desc;
	Component comp;

	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_Output;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	comp = FindNextComponent(NULL, &desc);
	if(!comp) {
		ERROR("Unable to open default OS X defice\n");
		return -1;
	}

	if(OpenAComponent(comp, &au) != noErr) {
		ERROR("Unable to open default OS X defice\n");
		return -1;
	}
	
	CloseComponent(au);*/

	return 0;
}

static int osx_initDriver(AudioOutput * audioOutput, ConfigParam * param) {
	OsxData * od  = newOsxData();

	audioOutput->data = od;

	return 0;
}

static void freeOsxData(OsxData * od) {
	free(od);
}

static void osx_finishDriver(AudioOutput * audioOutput) {
	OsxData * od = (OsxData *)audioOutput->data;
	freeOsxData(od);
}

static void osx_dropBufferedAudio(AudioOutput * audioOutput) {
	OsxData * od = (OsxData *)audioOutput->data;

	pthread_mutex_lock(&od->mutex);
	od->len = 0;
	od->go = 0;
	pthread_mutex_unlock(&od->mutex);
}

static void osx_closeDevice(AudioOutput * audioOutput) {
	OsxData * od = (OsxData *) audioOutput->data;

	pthread_mutex_lock(&od->mutex);
	od->go = 0;
	while(od->len) {
		pthread_cond_wait(&od->condition, &od->mutex);
	}
	pthread_mutex_unlock(&od->mutex);

	if(od->started) {
		AudioOutputUnitStop(od->au);
		od->started = 0;
	}

	CloseComponent(od->au);
	AudioUnitUninitialize(od->au);

	audioOutput->open = 0;
}

static OSStatus osx_render(void * vdata, 
		AudioUnitRenderActionFlags *ioActionFlags, 
		const AudioTimeStamp * inTimeStamp,
		UInt32 inBusNumber, UInt32 inNumberFrames,
		AudioBufferList *bufferList)
{
	OsxData * od = (OsxData *)vdata;
	AudioBuffer * buffer = &bufferList->mBuffers[0];
	int bufferSize = buffer->mDataByteSize;
	int bytesToCopy;
	int curpos = 0;

	pthread_mutex_lock(&od->mutex);

	while((od->go || od->len) && bufferSize) {
		while(od->go && od->len < bufferSize && 
				od->len < BUFFER_SIZE)
		{
			pthread_cond_signal(&od->condition);
			pthread_cond_wait(&od->condition, &od->mutex);
		}

		bytesToCopy = od->len < bufferSize ? od->len : bufferSize;
		bufferSize -= bytesToCopy;
		od->len -= bytesToCopy;

		if(od->pos+bytesToCopy > BUFFER_SIZE) {
			int bytes = BUFFER_SIZE-od->pos;
			memcpy(buffer->mData+curpos, od->buffer+od->pos, bytes);
			od->pos = 0;
			curpos += bytes;
			bytesToCopy -= bytes;
		}

		memcpy(buffer->mData+curpos, od->buffer+od->pos, bytesToCopy);
		od->pos += bytesToCopy;
		curpos += bytesToCopy;

		if(od->pos >= BUFFER_SIZE) od->pos = 0;
	}

	if(bufferSize) {
		memset(buffer->mData+curpos, 0, bufferSize);
	}

	pthread_cond_signal(&od->condition);
	pthread_mutex_unlock(&od->mutex);

	return 0;
}

static int osx_openDevice(AudioOutput * audioOutput) {
	OsxData * od = (OsxData *)audioOutput->data;
	ComponentDescription desc;
	Component comp;
	AURenderCallbackStruct callback;
	AudioFormat * audioFormat = &audioOutput->outAudioFormat;
	AudioStreamBasicDescription streamDesc;

	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	comp = FindNextComponent(NULL, &desc);
	if(comp == 0) {
		ERROR("Error finding OS X component\n");
		return -1;
	}

	if(OpenAComponent(comp, &od->au) != noErr) {
		ERROR("Unable to open OS X component\n");
		return -1;
	}

	if(AudioUnitInitialize(od->au) != 0) {
		CloseComponent(od->au);
		ERROR("Unable to initialuze OS X audio unit\n");
		return -1;
	}

	callback.inputProc = osx_render;
	callback.inputProcRefCon = od;

	if(AudioUnitSetProperty(od->au, kAudioUnitProperty_SetRenderCallback,
				kAudioUnitScope_Input, 0,
				&callback, sizeof(callback)) != 0)
	{
		AudioUnitUninitialize(od->au);
		CloseComponent(od->au);
		ERROR("unable to set callbak for OS X audio unit\n");
		return -1;
	}

	streamDesc.mSampleRate = audioFormat->sampleRate;
	streamDesc.mFormatID = kAudioFormatLinearPCM;
	streamDesc.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger |
			    kLinearPCMFormatFlagIsBigEndian;
	streamDesc.mBytesPerPacket = audioFormat->channels*audioFormat->bits/8;
	streamDesc.mFramesPerPacket = 1;
	streamDesc.mBytesPerFrame = streamDesc.mBytesPerPacket;
	streamDesc.mChannelsPerFrame = audioFormat->channels;
	streamDesc.mBitsPerChannel = audioFormat->bits;

	if(AudioUnitSetProperty(od->au, kAudioUnitProperty_StreamFormat,
				kAudioUnitScope_Input, 0,
				&streamDesc, sizeof(streamDesc)) != 0)
	{
		AudioUnitUninitialize(od->au);
		CloseComponent(od->au);
		ERROR("Unable to set format on OS X device\n");
		return -1;
	}

	od->pos = 0;
	od->len = 0;

	audioOutput->open = 1;

	return 0;
}

static int osx_play(AudioOutput * audioOutput, char * playChunk, int size) {
	OsxData * od = (OsxData *)audioOutput->data;
	int bytesToCopy;
	int curpos;

	if(!od->started) {
		od->go = 1;
		od->started = 1;
		int err = AudioOutputUnitStart(od->au);
		if(err) {
			ERROR("unable to start audio output: %i\n", err);
			return -1;
		}
	}

	pthread_mutex_lock(&od->mutex);

	curpos = od->pos+od->len;
	if(curpos >= BUFFER_SIZE) curpos -= BUFFER_SIZE;

	while(size) {
		while(od->len >= BUFFER_SIZE) {
			pthread_cond_signal(&od->condition);
			pthread_cond_wait(&od->condition, &od->mutex);
		}

		bytesToCopy = BUFFER_SIZE - od->len;
		bytesToCopy = bytesToCopy < size ? bytesToCopy : size;
		size -= bytesToCopy;
		od->len += bytesToCopy;

		if(curpos+bytesToCopy > BUFFER_SIZE) {
			int bytes = BUFFER_SIZE-curpos;
			memcpy(od->buffer+curpos, playChunk, bytes);
			curpos = 0;
			playChunk += bytes;
			bytesToCopy -= bytes;
		}

		memcpy(od->buffer+curpos, playChunk, bytesToCopy);
		curpos += bytesToCopy;
		playChunk += bytesToCopy;

		if(curpos >= BUFFER_SIZE) curpos = 0;
	}

	pthread_cond_signal(&od->condition);
	pthread_mutex_unlock(&od->mutex);

	return 0;
}

AudioOutputPlugin osxPlugin = 
{
	"osx",
	osx_testDefault,
	osx_initDriver,
	osx_finishDriver,
	osx_openDevice,
	osx_play,
	osx_dropBufferedAudio,
	osx_closeDevice,
	NULL /* sendMetadataFunc */
};

#else

#include <stdio.h>

AudioOutputPlugin osxPlugin = 
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

#endif
