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

#include "../conf.h"
#include "../log.h"

#include <CoreAudio/AudioHardware.h>
#include <stdlib.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

typedef struct _OsxData {
	AudioDeviceID deviceID;
	AudioStreamBasicDescription streamDesc;
	pthread_mutex_t	mutex;
	pthread_cond_t condition;
	Float32 buffer[BUFFER_SIZE];
	int pos;
	int len;
	int go;
	int started;
} OsxData;

static void printError(OSStatus val) {
	switch(val) {
	case kAudioHardwareNoError:
		ERROR("kAudioHardwareNoErr");
		break;
	case kAudioHardwareNotRunningError:
		ERROR("kAudioHardwareNotRunningError");
		break;
	case kAudioHardwareUnspecifiedError:
		ERROR("kAudioHardwareUnspecifiedError");
		break;
	case kAudioHardwareUnknownPropertyError:
		ERROR("kAudioHardwareUnknownPropertyError");
		break;
	case kAudioHardwareBadPropertySizeError:
		ERROR("kAudioHardwareBadPropertySizeError");
		break;
	case kAudioHardwareIllegalOperationError:
		ERROR("kAudioHardwareIllegalOperationError");
		break;
	case kAudioHardwareBadDeviceError:
		ERROR("kAudioHardwareBadDeviceError");
		break;
	case kAudioHardwareBadStreamError:
		ERROR("kAudioHardwareBadStreamError");
		break;
	case kAudioHardwareUnsupportedOperationError:
		ERROR("kAudioHardwareUnsupportedOperationError");
		break;
	case kAudioDeviceUnsupportedFormatError:
		ERROR("kAudioDeviceUnsupportedFormatError");
		break;
	case kAudioDevicePermissionsError:
		ERROR("kAudioDevicePermissionsError");
		break;
	default:
		ERROR("unknown");
		break;
	}
}

static OsxData * newOsxData() {
	OsxData * ret = malloc(sizeof(OsxData));

	ret->deviceID = kAudioDeviceUnknown;

	pthread_mutex_init(&ret->mutex, NULL);
	pthread_cond_init(&ret->condition, NULL);

	ret->pos = 0;
	ret->len = 0;
	ret->go = 0;
	ret->started = 0;

	return ret;
}

static int osx_testDefault() {
	int err;
	AudioDeviceID deviceID;
	UInt32 propertySize = sizeof(deviceID);

	err = AudioHardwareGetProperty(
			kAudioHardwarePropertyDefaultOutputDevice,
			&propertySize, &deviceID);
	if(err || deviceID == kAudioDeviceUnknown) {
		WARNING("Not able to get the default OS X device\n");
		return -1;
	}

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
	/* not implemented yet */
}

static void osx_closeDevice(AudioOutput * audioOutput) {
	//OsxData * od = (OsxData *) audioOutput->data;

	audioOutput->open = 0;
}

static OSStatus osx_IOProc(AudioDeviceID deviceID, 
	const AudioTimeStamp * inNow, const AudioBufferList *inData,
	const AudioTimeStamp * inInputTime, AudioBufferList *outData,
	const AudioTimeStamp * inOutputTime, void * vdata)
{
	OsxData * od = (OsxData *)vdata;
	AudioBuffer * buffer = &outData->mBuffers[0];
	int bufferSize = buffer->mDataByteSize/sizeof(Float32);
	int floatsToCopy;
	int curpos = 0;

	DEBUG("entering IOProc\n");

	pthread_mutex_lock(&od->mutex);

	while((od->go || od->len) && bufferSize) {
		while(od->go && od->len < bufferSize && 
				od->len < BUFFER_SIZE)
		{
			pthread_cond_wait(&od->condition, &od->mutex);
		}

		floatsToCopy = od->len < bufferSize ? od->len : bufferSize;
		bufferSize -= floatsToCopy;

		if(od->pos+floatsToCopy > BUFFER_SIZE) {
			int floats = BUFFER_SIZE-od->pos;
			memcpy(buffer->mData+curpos, od->buffer+od->pos, 
					floats*sizeof(Float32));
			od->len -= floats;
			od->pos = 0;
			curpos += floats;
			floatsToCopy -= floats;
		}

		memcpy(buffer->mData+curpos, od->buffer+od->pos, 
				floatsToCopy*sizeof(Float32));
		od->len -= floatsToCopy;
		od->pos += floatsToCopy;
		curpos += floatsToCopy;
	}

	if(bufferSize) {
		memset(buffer->mData+curpos, 0, bufferSize*sizeof(Float32));
	}

	pthread_mutex_unlock(&od->mutex);
	pthread_cond_signal(&od->condition);

	DEBUG("exiting IOProc\n");

	return 0;
}

static int osx_openDevice(AudioOutput * audioOutput) {
	int err;
	OsxData * od = (OsxData *)audioOutput->data;
	UInt32 propertySize;
	AudioFormat * audioFormat = &audioOutput->outAudioFormat;
	UInt32 bufferByteCount = 8192;

	propertySize = sizeof(od->deviceID);
	err = AudioHardwareGetProperty(
			kAudioHardwarePropertyDefaultOutputDevice,
			&propertySize, &od->deviceID);
	if(err || od->deviceID == kAudioDeviceUnknown) {
		ERROR("Not able to get the default OS X device\n");
		return -1;
	}

	od->streamDesc.mFormatID = kAudioFormatLinearPCM;
	od->streamDesc.mSampleRate = audioFormat->sampleRate;
	od->streamDesc.mFormatFlags = kLinearPCMFormatFlagIsFloat |
				      kLinearPCMFormatFlagIsBigEndian |
				      kLinearPCMFormatFlagIsPacked;
	od->streamDesc.mBytesPerPacket = audioFormat->channels*sizeof(Float32);
	od->streamDesc.mFramesPerPacket = 1;
	od->streamDesc.mBytesPerFrame = audioFormat->channels*sizeof(Float32);
	od->streamDesc.mChannelsPerFrame = audioFormat->channels;
	od->streamDesc.mBitsPerChannel = 8 * sizeof(Float32);

	audioFormat->bits = 16;

	propertySize = sizeof(od->streamDesc);
	err = AudioDeviceSetProperty(od->deviceID, 0, 0, false, 
			kAudioDevicePropertyStreamFormat,
			propertySize, &od->streamDesc);
	if(err) {
		ERROR("unable to set format %i:%i:% on osx device\n",
				(int)audioFormat->sampleRate,
				(int)audioFormat->bits,
				(int)audioFormat->channels);
		return -1;
	}

	propertySize = sizeof(UInt32);
	err = AudioDeviceSetProperty(od->deviceID, 0, 0, false, 
			kAudioDevicePropertyBufferSize,
        		propertySize, &bufferByteCount);

	err = AudioDeviceAddIOProc(od->deviceID, osx_IOProc, od);
	if(err) {
		ERROR("error adding IOProc\n");
		return -1;
	}

	od->go = 1;
	od->pos = 0;
	od->len = 0;

	audioOutput->open = 1;

	return 0;
}

static void copyIntBufferToFloat(char * playChunk, int size, float * buffer,
		int floats) 
{
	/* this is for 16-bit audio only */
	SInt16 * sample;

	while(floats) {
		sample = (SInt16 *)playChunk;
		*buffer = *sample/32767.0;
		playChunk += 2;
		buffer++;
		floats--;
	}
}

static int osx_play(AudioOutput * audioOutput, char * playChunk, int size) {
	OsxData * od = (OsxData *)audioOutput->data;
	int floatsToCopy;

	size /= 2;

	DEBUG("entering osx_play\n");

	pthread_mutex_lock(&od->mutex);

	DEBUG("entering while loop\n");
	while(size) {
		DEBUG("iterating loop with size = %i\n", size);
		while(od->len == BUFFER_SIZE) {
			if(!od->started) {
				OSStatus err = AudioDeviceStart(od->deviceID,
							osx_IOProc);
				DEBUG("start audio device\n");
				if(err) {
					printError(err);
					ERROR(" error doing AudioDeviceStart "
							"for osx device: %i\n",
							(int)err);
					pthread_mutex_unlock(&od->mutex);
					return -1;
				}
				od->started = 1;
				DEBUG("audio device started\n");
			}

			DEBUG("cond_wait\n");
			pthread_cond_wait(&od->condition, &od->mutex);
		}

		floatsToCopy = BUFFER_SIZE - od->len;
		floatsToCopy = floatsToCopy < size ? floatsToCopy : size;
		size -= floatsToCopy;

		if(od->pos+floatsToCopy > BUFFER_SIZE) {
			int floats = BUFFER_SIZE-od->pos;
			copyIntBufferToFloat(playChunk, 
					audioOutput->outAudioFormat.bits/8, 
					od->buffer,
					floats);
			od->pos = 0;
			od->len += floats;
			playChunk += floats*sizeof(Float32);
		}

		copyIntBufferToFloat(playChunk, 
				audioOutput->outAudioFormat.bits/8, 
				od->buffer,
				floatsToCopy);
		od->pos += floatsToCopy;
		od->len += floatsToCopy;
		playChunk += floatsToCopy*sizeof(Float32);
	}

	pthread_mutex_unlock(&od->mutex);
	pthread_cond_signal(&od->condition);

	DEBUG("exiting osx_play\n");
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
