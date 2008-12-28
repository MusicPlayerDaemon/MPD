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

#include "../output_api.h"
#include "../utils.h"

#include <glib.h>
#include <AudioUnit/AudioUnit.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "osx"

typedef struct _OsxData {
	AudioUnit au;
	GMutex *mutex;
	GCond *condition;
	char *buffer;
	size_t bufferSize;
	size_t pos;
	size_t len;
	int started;
} OsxData;

static OsxData *newOsxData(void)
{
	OsxData *ret = g_new(OsxData, 1);

	ret->mutex = g_mutex_new();
	ret->condition = g_cond_new();

	ret->pos = 0;
	ret->len = 0;
	ret->started = 0;
	ret->buffer = NULL;
	ret->bufferSize = 0;

	return ret;
}

static bool osx_testDefault(void)
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

	return true;
}

static void *
osx_initDriver(mpd_unused struct audio_output *audioOutput,
	       mpd_unused const struct audio_format *audio_format,
	       mpd_unused ConfigParam * param)
{
	return newOsxData();
}

static void freeOsxData(OsxData * od)
{
	if (od->buffer)
		free(od->buffer);
	g_mutex_free(od->mutex);
	g_cond_free(od->condition);
	free(od);
}

static void osx_finishDriver(void *data)
{
	OsxData *od = data;
	freeOsxData(od);
}

static void osx_dropBufferedAudio(void *data)
{
	OsxData *od = data;

	g_mutex_lock(od->mutex);
	od->len = 0;
	g_mutex_unlock(od->mutex);
}

static void osx_closeDevice(void *data)
{
	OsxData *od = data;

	g_mutex_lock(od->mutex);
	while (od->len) {
		g_cond_wait(od->condition, od->mutex);
	}
	g_mutex_unlock(od->mutex);

	if (od->started) {
		AudioOutputUnitStop(od->au);
		od->started = 0;
	}

	AudioUnitUninitialize(od->au);
	CloseComponent(od->au);
}

static OSStatus
osx_render(void *vdata,
	   mpd_unused AudioUnitRenderActionFlags * ioActionFlags,
	   mpd_unused const AudioTimeStamp * inTimeStamp,
	   mpd_unused UInt32 inBusNumber, mpd_unused UInt32 inNumberFrames,
	   AudioBufferList * bufferList)
{
	OsxData *od = (OsxData *) vdata;
	AudioBuffer *buffer = &bufferList->mBuffers[0];
	size_t bufferSize = buffer->mDataByteSize;
	size_t bytesToCopy;
	size_t bytes;
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
	g_mutex_lock(od->mutex);
	/*
	   DEBUG("%i:%i\n", bufferSize, od->len);
	   while(od->go && od->len < bufferSize && 
	   od->len < od->bufferSize)
	   {
	   DEBUG("osx_render: wait\n");
	   g_cond_wait(od->condition, od->mutex);
	   }
	 */

	bytesToCopy = MIN(od->len, bufferSize);
	bufferSize = bytesToCopy;
	od->len -= bytesToCopy;

	bytes = od->bufferSize - od->pos;
	if (bytesToCopy > bytes) {
		memcpy((unsigned char*)buffer->mData + curpos, od->buffer + od->pos, bytes);
		od->pos = 0;
		curpos += bytes;
		bytesToCopy -= bytes;
	}

	memcpy((unsigned char*)buffer->mData + curpos, od->buffer + od->pos, bytesToCopy);
	od->pos += bytesToCopy;

	if (od->pos >= od->bufferSize)
		od->pos = 0;
	/* DEBUG("osx_render: unlock\n"); */
	g_mutex_unlock(od->mutex);
	g_cond_signal(od->condition);
	/* } */

	buffer->mDataByteSize = bufferSize;

	if (!bufferSize) {
		my_usleep(1000);
	}

	/* DEBUG("osx_render: leave\n"); */
	return 0;
}

static bool
osx_openDevice(void *data, struct audio_format *audioFormat)
{
	OsxData *od = data;
	ComponentDescription desc;
	Component comp;
	AURenderCallbackStruct callback;
	AudioStreamBasicDescription streamDesc;

	if (audioFormat->bits > 16)
		audioFormat->bits = 16;

	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	comp = FindNextComponent(NULL, &desc);
	if (comp == 0) {
		g_warning("Error finding OS X component\n");
		return false;
	}

	if (OpenAComponent(comp, &od->au) != noErr) {
		g_warning("Unable to open OS X component\n");
		return false;
	}

	if (AudioUnitInitialize(od->au) != 0) {
		CloseComponent(od->au);
		g_warning("Unable to initialize OS X audio unit\n");
		return false;
	}

	callback.inputProc = osx_render;
	callback.inputProcRefCon = od;

	if (AudioUnitSetProperty(od->au, kAudioUnitProperty_SetRenderCallback,
				 kAudioUnitScope_Input, 0,
				 &callback, sizeof(callback)) != 0) {
		AudioUnitUninitialize(od->au);
		CloseComponent(od->au);
		g_warning("unable to set callback for OS X audio unit\n");
		return false;
	}

	streamDesc.mSampleRate = audioFormat->sample_rate;
	streamDesc.mFormatID = kAudioFormatLinearPCM;
	streamDesc.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
#ifdef WORDS_BIGENDIAN
	streamDesc.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
#endif

	streamDesc.mBytesPerPacket = audio_format_frame_size(audioFormat);
	streamDesc.mFramesPerPacket = 1;
	streamDesc.mBytesPerFrame = streamDesc.mBytesPerPacket;
	streamDesc.mChannelsPerFrame = audioFormat->channels;
	streamDesc.mBitsPerChannel = audioFormat->bits;

	if (AudioUnitSetProperty(od->au, kAudioUnitProperty_StreamFormat,
				 kAudioUnitScope_Input, 0,
				 &streamDesc, sizeof(streamDesc)) != 0) {
		AudioUnitUninitialize(od->au);
		CloseComponent(od->au);
		g_warning("Unable to set format on OS X device\n");
		return false;
	}

	/* create a buffer of 1s */
	od->bufferSize = (audioFormat->sample_rate) *
		audio_format_frame_size(audioFormat);
	od->buffer = g_realloc(od->buffer, od->bufferSize);

	od->pos = 0;
	od->len = 0;

	return true;
}

static bool
osx_play(void *data, const char *playChunk, size_t size)
{
	OsxData *od = data;
	size_t bytesToCopy;
	size_t bytes;
	size_t curpos;

	/* DEBUG("osx_play: enter\n"); */

	if (!od->started) {
		int err;
		od->started = 1;
		err = AudioOutputUnitStart(od->au);
		if (err) {
			g_warning("unable to start audio output: %i\n", err);
			return false;
		}
	}

	g_mutex_lock(od->mutex);

	while (size) {
		/* DEBUG("osx_play: lock\n"); */
		curpos = od->pos + od->len;
		if (curpos >= od->bufferSize)
			curpos -= od->bufferSize;

		bytesToCopy = MIN(od->bufferSize, size);

		while (od->len > od->bufferSize - bytesToCopy) {
			/* DEBUG("osx_play: wait\n"); */
			g_cond_wait(od->condition, od->mutex);
		}

		size -= bytesToCopy;
		od->len += bytesToCopy;

		bytes = od->bufferSize - curpos;
		if (bytesToCopy > bytes) {
			memcpy(od->buffer + curpos, playChunk, bytes);
			curpos = 0;
			playChunk += bytes;
			bytesToCopy -= bytes;
		}

		memcpy(od->buffer + curpos, playChunk, bytesToCopy);
		playChunk += bytesToCopy;

	}
	/* DEBUG("osx_play: unlock\n"); */
	g_mutex_unlock(od->mutex);

	/* DEBUG("osx_play: leave\n"); */
	return true;
}

const struct audio_output_plugin osxPlugin = {
	.name = "osx",
	.test_default_device = osx_testDefault,
	.init = osx_initDriver,
	.finish = osx_finishDriver,
	.open = osx_openDevice,
	.play = osx_play,
	.cancel = osx_dropBufferedAudio,
	.close = osx_closeDevice,
};
