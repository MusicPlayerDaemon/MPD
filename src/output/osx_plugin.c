/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "output_api.h"

#include <glib.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreServices/CoreServices.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "osx"

struct osx_output {
	AudioUnit au;
	GMutex *mutex;
	GCond *condition;
	char *buffer;
	size_t buffer_size;
	size_t pos;
	size_t len;
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
osx_output_quark(void)
{
	return g_quark_from_static_string("osx_output");
}

static bool
osx_output_test_default_device(void)
{
	/* on a Mac, this is always the default plugin, if nothing
	   else is configured */
	return true;
}

static void *
osx_output_init(G_GNUC_UNUSED const struct audio_format *audio_format,
		G_GNUC_UNUSED const struct config_param *param,
		G_GNUC_UNUSED GError **error)
{
	struct osx_output *oo = g_new(struct osx_output, 1);

	oo->mutex = g_mutex_new();
	oo->condition = g_cond_new();

	oo->pos = 0;
	oo->len = 0;
	oo->buffer = NULL;
	oo->buffer_size = 0;

	return oo;
}

static void osx_output_finish(void *data)
{
	struct osx_output *od = data;

	g_free(od->buffer);
	g_mutex_free(od->mutex);
	g_cond_free(od->condition);
	g_free(od);
}

static void osx_output_cancel(void *data)
{
	struct osx_output *od = data;

	g_mutex_lock(od->mutex);
	od->len = 0;
	g_mutex_unlock(od->mutex);
}

static void osx_output_close(void *data)
{
	struct osx_output *od = data;

	g_mutex_lock(od->mutex);
	while (od->len) {
		g_cond_wait(od->condition, od->mutex);
	}
	g_mutex_unlock(od->mutex);

	AudioOutputUnitStop(od->au);
	AudioUnitUninitialize(od->au);
	CloseComponent(od->au);
}

static OSStatus
osx_render(void *vdata,
	   G_GNUC_UNUSED AudioUnitRenderActionFlags *io_action_flags,
	   G_GNUC_UNUSED const AudioTimeStamp *in_timestamp,
	   G_GNUC_UNUSED UInt32 in_bus_number,
	   G_GNUC_UNUSED UInt32 in_number_frames,
	   AudioBufferList *buffer_list)
{
	struct osx_output *od = (struct osx_output *) vdata;
	AudioBuffer *buffer = &buffer_list->mBuffers[0];
	size_t buffer_size = buffer->mDataByteSize;
	size_t bytes_to_copy;
	size_t trailer_length;
	size_t dest_pos = 0;

	g_mutex_lock(od->mutex);

	bytes_to_copy = MIN(od->len, buffer_size);
	buffer_size = bytes_to_copy;
	od->len -= bytes_to_copy;

	trailer_length = od->buffer_size - od->pos;
	if (bytes_to_copy > trailer_length) {
		memcpy((unsigned char*)buffer->mData + dest_pos,
		       od->buffer + od->pos, trailer_length);
		od->pos = 0;
		dest_pos += trailer_length;
		bytes_to_copy -= trailer_length;
	}

	memcpy((unsigned char*)buffer->mData + dest_pos,
	       od->buffer + od->pos, bytes_to_copy);
	od->pos += bytes_to_copy;

	if (od->pos >= od->buffer_size)
		od->pos = 0;

	g_mutex_unlock(od->mutex);
	g_cond_signal(od->condition);

	buffer->mDataByteSize = buffer_size;

	if (!buffer_size) {
		g_usleep(1000);
	}

	return 0;
}

static bool
osx_output_open(void *data, struct audio_format *audio_format, GError **error)
{
	struct osx_output *od = data;
	ComponentDescription desc;
	Component comp;
	AURenderCallbackStruct callback;
	AudioStreamBasicDescription stream_description;
	OSStatus status;
	ComponentResult result;

	if (audio_format->bits > 16)
		audio_format->bits = 16;

	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	comp = FindNextComponent(NULL, &desc);
	if (comp == 0) {
		g_set_error(error, osx_output_quark(), 0,
			    "Error finding OS X component");
		return false;
	}

	status = OpenAComponent(comp, &od->au);
	if (status != noErr) {
		g_set_error(error, osx_output_quark(), 0,
			    "Unable to open OS X component: %s",
			    GetMacOSStatusCommentString(status));
		return false;
	}

	status = AudioUnitInitialize(od->au);
	if (status != noErr) {
		CloseComponent(od->au);
		g_set_error(error, osx_output_quark(), 0,
			    "Unable to initialize OS X audio unit: %s",
			    GetMacOSStatusCommentString(status));
		return false;
	}

	callback.inputProc = osx_render;
	callback.inputProcRefCon = od;

	result = AudioUnitSetProperty(od->au,
				      kAudioUnitProperty_SetRenderCallback,
				      kAudioUnitScope_Input, 0,
				      &callback, sizeof(callback));
	if (result != noErr) {
		AudioUnitUninitialize(od->au);
		CloseComponent(od->au);
		g_set_error(error, osx_output_quark(), 0,
			    "unable to set callback for OS X audio unit");
		return false;
	}

	stream_description.mSampleRate = audio_format->sample_rate;
	stream_description.mFormatID = kAudioFormatLinearPCM;
	stream_description.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
#if G_BYTE_ORDER == G_BIG_ENDIAN
	stream_description.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
#endif

	stream_description.mBytesPerPacket =
		audio_format_frame_size(audio_format);
	stream_description.mFramesPerPacket = 1;
	stream_description.mBytesPerFrame = stream_description.mBytesPerPacket;
	stream_description.mChannelsPerFrame = audio_format->channels;
	stream_description.mBitsPerChannel = audio_format->bits;

	result = AudioUnitSetProperty(od->au, kAudioUnitProperty_StreamFormat,
				      kAudioUnitScope_Input, 0,
				      &stream_description,
				      sizeof(stream_description));
	if (result != noErr) {
		AudioUnitUninitialize(od->au);
		CloseComponent(od->au);
		g_set_error(error, osx_output_quark(), 0,
			    "Unable to set format on OS X device");
		return false;
	}

	/* create a buffer of 1s */
	od->buffer_size = (audio_format->sample_rate) *
		audio_format_frame_size(audio_format);
	od->buffer = g_realloc(od->buffer, od->buffer_size);

	od->pos = 0;
	od->len = 0;

	status = AudioOutputUnitStart(od->au);
	if (status != 0) {
		g_set_error(error, osx_output_quark(), 0,
			    "unable to start audio output: %s",
			    GetMacOSStatusCommentString(status));
		return false;
	}

	return true;
}

static size_t
osx_output_play(void *data, const void *chunk, size_t size,
		G_GNUC_UNUSED GError **error)
{
	struct osx_output *od = data;
	size_t start, nbytes;

	g_mutex_lock(od->mutex);

	while (od->len >= od->buffer_size)
		/* wait for some free space in the buffer */
		g_cond_wait(od->condition, od->mutex);

	start = od->pos + od->len;
	if (start >= od->buffer_size)
		start -= od->buffer_size;

	nbytes = start < od->pos
		? od->pos - start
		: od->buffer_size - start;

	assert(nbytes > 0);

	if (nbytes > size)
		nbytes = size;

	memcpy(od->buffer + start, chunk, nbytes);
	od->len += nbytes;

	g_mutex_unlock(od->mutex);

	return nbytes;
}

const struct audio_output_plugin osxPlugin = {
	.name = "osx",
	.test_default_device = osx_output_test_default_device,
	.init = osx_output_init,
	.finish = osx_output_finish,
	.open = osx_output_open,
	.close = osx_output_close,
	.play = osx_output_play,
	.cancel = osx_output_cancel,
};
