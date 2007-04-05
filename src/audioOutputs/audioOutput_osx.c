/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

typedef struct _OsxData {
	AudioUnit au;
	pthread_mutex_t mutex;
	pthread_cond_t condition;
	char *buffer;
	int bufferSize;
	int pos;
	int len;
	int started;
} OsxData;

static OsxData *newOsxData()
{
	OsxData *ret = xmalloc(sizeof(OsxData));

	pthread_mutex_init(&ret->mutex, NULL);
	pthread_cond_init(&ret->condition, NULL);

	ret->pos = 0;
	ret->len = 0;
	ret->started = 0;
	ret->buffer = NULL;
	ret->bufferSize = 0;

	return ret;
}

static int osx_testDefault()
{
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

	   CloseComponent(au); */

	return 0;
}

static int osx_initDriver(AudioOutput * audioOutput, ConfigParam * param)
{
	OsxData *od = newOsxData();

	audioOutput->data = od;

	return 0;
}

static void freeOsxData(OsxData * od)
{
	if (od->buffer)
		free(od->buffer);
	pthread_mutex_destroy(&od->mutex);
	pthread_cond_destroy(&od->condition);
	free(od);
}

static void osx_finishDriver(AudioOutput * audioOutput)
{
	OsxData *od = (OsxData *) audioOutput->data;
	freeOsxData(od);
}

static void osx_dropBufferedAudio(AudioOutput * audioOutput)
{
	OsxData *od = (OsxData *) audioOutput->data;

	pthread_mutex_lock(&od->mutex);
	od->len = 0;
	pthread_mutex_unlock(&od->mutex);
}

static void osx_closeDevice(AudioOutput * audioOutput)
{
	OsxData *od = (OsxData *) audioOutput->data;

	pthread_mutex_lock(&od->mutex);
	while (od->len) {
		pthread_cond_wait(&od->condition, &od->mutex);
	}
	pthread_mutex_unlock(&od->mutex);

	if (od->started) {
		AudioOutputUnitStop(od->au);
		od->started = 0;
	}

	CloseComponent(od->au);
	AudioUnitUninitialize(od->au);

	audioOutput->open = 0;
}

static OSStatus osx_render(void *vdata,
			   AudioUnitRenderActionFlags * ioActionFlags,
			   const AudioTimeStamp * inTimeStamp,
			   UInt32 inBusNumber, UInt32 inNumberFrames,
			   AudioBufferList * bufferList)
{
	OsxData *od = (OsxData *) vdata;
	AudioBuffer *buffer = &bufferList->mBuffers[0];
	int bufferSize = buffer->mDataByteSize;
	int bytesToCopy;
	int curpos = 0;

	/*DEBUG("osx_render: enter : %i\n", (int)bufferList->mNumberBuffers);
	   DEBUG("osx_render: ioActionFlags: %p\n", ioActionFlags);
	   if(ioActionFlags) {
	   if(*ioActionFlags & kAudioUnitRenderAction_PreRender) {
	   DEBUG("prerender\n");
	   }
	   if(*ioActionFlags & kAudioUnitRenderAction_PostRender) {
	   DEBUG("post render\n");
	   }
	   if(*ioActionFlags & kAudioUnitRenderAction_OutputIsSilence) {
	   DEBUG("post render\n");
	   }
	   if(*ioActionFlags & kAudioOfflineUnitRenderAction_Preflight) {
	   DEBUG("prefilight\n");
	   }
	   if(*ioActionFlags & kAudioOfflineUnitRenderAction_Render) {
	   DEBUG("render\n");
	   }
	   if(*ioActionFlags & kAudioOfflineUnitRenderAction_Complete) {
	   DEBUG("complete\n");
	   }
	   } */

	/* while(bufferSize) {
	   DEBUG("osx_render: lock\n"); */
	pthread_mutex_lock(&od->mutex);
	/*
	   DEBUG("%i:%i\n", bufferSize, od->len);
	   while(od->go && od->len < bufferSize && 
	   od->len < od->bufferSize)
	   {
	   DEBUG("osx_render: wait\n");
	   pthread_cond_wait(&od->condition, &od->mutex);
	   }
	 */

	bytesToCopy = od->len < bufferSize ? od->len : bufferSize;
	bufferSize = bytesToCopy;
	od->len -= bytesToCopy;

	if (od->pos + bytesToCopy > od->bufferSize) {
		int bytes = od->bufferSize - od->pos;
		memcpy(buffer->mData + curpos, od->buffer + od->pos, bytes);
		od->pos = 0;
		curpos += bytes;
		bytesToCopy -= bytes;
	}

	memcpy(buffer->mData + curpos, od->buffer + od->pos, bytesToCopy);
	od->pos += bytesToCopy;
	curpos += bytesToCopy;

	if (od->pos >= od->bufferSize)
		od->pos = 0;
	/* DEBUG("osx_render: unlock\n"); */
	pthread_mutex_unlock(&od->mutex);
	pthread_cond_signal(&od->condition);
	/* } */

	buffer->mDataByteSize = bufferSize;

	if (!bufferSize) {
		my_usleep(1000);
	}

	/* DEBUG("osx_render: leave\n"); */
	return 0;
}

static int osx_openDevice(AudioOutput * audioOutput)
{
	OsxData *od = (OsxData *) audioOutput->data;
	ComponentDescription desc;
	Component comp;
	AURenderCallbackStruct callback;
	AudioFormat *audioFormat = &audioOutput->outAudioFormat;
	AudioStreamBasicDescription streamDesc;

	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	comp = FindNextComponent(NULL, &desc);
	if (comp == 0) {
		ERROR("Error finding OS X component\n");
		return -1;
	}

	if (OpenAComponent(comp, &od->au) != noErr) {
		ERROR("Unable to open OS X component\n");
		return -1;
	}

	if (AudioUnitInitialize(od->au) != 0) {
		CloseComponent(od->au);
		ERROR("Unable to initialize OS X audio unit\n");
		return -1;
	}

	callback.inputProc = osx_render;
	callback.inputProcRefCon = od;

	if (AudioUnitSetProperty(od->au, kAudioUnitProperty_SetRenderCallback,
				 kAudioUnitScope_Input, 0,
				 &callback, sizeof(callback)) != 0) {
		AudioUnitUninitialize(od->au);
		CloseComponent(od->au);
		ERROR("unable to set callback for OS X audio unit\n");
		return -1;
	}

	streamDesc.mSampleRate = audioFormat->sampleRate;
	streamDesc.mFormatID = kAudioFormatLinearPCM;
	streamDesc.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
#ifdef WORDS_BIGENDIAN
	streamDesc.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
#endif

	streamDesc.mBytesPerPacket =
	    audioFormat->channels * audioFormat->bits / 8;
	streamDesc.mFramesPerPacket = 1;
	streamDesc.mBytesPerFrame = streamDesc.mBytesPerPacket;
	streamDesc.mChannelsPerFrame = audioFormat->channels;
	streamDesc.mBitsPerChannel = audioFormat->bits;

	if (AudioUnitSetProperty(od->au, kAudioUnitProperty_StreamFormat,
				 kAudioUnitScope_Input, 0,
				 &streamDesc, sizeof(streamDesc)) != 0) {
		AudioUnitUninitialize(od->au);
		CloseComponent(od->au);
		ERROR("Unable to set format on OS X device\n");
		return -1;
	}

	/* create a buffer of 1s */
	od->bufferSize = (audioFormat->sampleRate) *
	    (audioFormat->bits >> 3) * (audioFormat->channels);
	od->buffer = xrealloc(od->buffer, od->bufferSize);

	od->pos = 0;
	od->len = 0;

	audioOutput->open = 1;

	return 0;
}

static int osx_play(AudioOutput * audioOutput, char *playChunk, int size)
{
	OsxData *od = (OsxData *) audioOutput->data;
	int bytesToCopy;
	int curpos;

	/* DEBUG("osx_play: enter\n"); */

	if (!od->started) {
		int err;
		od->started = 1;
		err = AudioOutputUnitStart(od->au);
		if (err) {
			ERROR("unable to start audio output: %i\n", err);
			return -1;
		}
	}

	pthread_mutex_lock(&od->mutex);

	while (size) {
		/* DEBUG("osx_play: lock\n"); */
		curpos = od->pos + od->len;
		if (curpos >= od->bufferSize)
			curpos -= od->bufferSize;

		bytesToCopy = od->bufferSize < size ? od->bufferSize : size;

		while (od->len > od->bufferSize - bytesToCopy) {
			/* DEBUG("osx_play: wait\n"); */
			pthread_cond_wait(&od->condition, &od->mutex);
		}

		bytesToCopy = od->bufferSize - od->len;
		bytesToCopy = bytesToCopy < size ? bytesToCopy : size;
		size -= bytesToCopy;
		od->len += bytesToCopy;

		if (curpos + bytesToCopy > od->bufferSize) {
			int bytes = od->bufferSize - curpos;
			memcpy(od->buffer + curpos, playChunk, bytes);
			curpos = 0;
			playChunk += bytes;
			bytesToCopy -= bytes;
		}

		memcpy(od->buffer + curpos, playChunk, bytesToCopy);
		curpos += bytesToCopy;
		playChunk += bytesToCopy;

	}
	/* DEBUG("osx_play: unlock\n"); */
	pthread_mutex_unlock(&od->mutex);

	/* DEBUG("osx_play: leave\n"); */
	return 0;
}

AudioOutputPlugin osxPlugin = {
	"osx",
	osx_testDefault,
	osx_initDriver,
	osx_finishDriver,
	osx_openDevice,
	osx_play,
	osx_dropBufferedAudio,
	osx_closeDevice,
	NULL,	/* sendMetadataFunc */
};

#else

#include <stdio.h>

DISABLED_AUDIO_OUTPUT_PLUGIN(osxPlugin)
#endif
