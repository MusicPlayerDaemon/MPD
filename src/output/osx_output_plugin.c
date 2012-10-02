/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "osx_output_plugin.h"
#include "output_api.h"
#include "fifo_buffer.h"

#include <glib.h>
#include <CoreAudio/AudioHardware.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreServices/CoreServices.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "osx"

struct osx_output {
	struct audio_output base;

	/* configuration settings */
	OSType component_subtype;
	/* only applicable with kAudioUnitSubType_HALOutput */
	const char *device_name;

	AudioUnit au;
	GMutex *mutex;
	GCond *condition;

	struct fifo_buffer *buffer;
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

static void
osx_output_configure(struct osx_output *oo, const struct config_param *param)
{
	const char *device = config_get_block_string(param, "device", NULL);

	if (device == NULL || 0 == strcmp(device, "default")) {
		oo->component_subtype = kAudioUnitSubType_DefaultOutput;
		oo->device_name = NULL;
	}
	else if (0 == strcmp(device, "system")) {
		oo->component_subtype = kAudioUnitSubType_SystemOutput;
		oo->device_name = NULL;
	}
	else {
		oo->component_subtype = kAudioUnitSubType_HALOutput;
		/* XXX am I supposed to g_strdup() this? */
		oo->device_name = device;
	}
}

static struct audio_output *
osx_output_init(const struct config_param *param, GError **error_r)
{
	struct osx_output *oo = g_new(struct osx_output, 1);
	if (!ao_base_init(&oo->base, &osx_output_plugin, param, error_r)) {
		g_free(oo);
		return NULL;
	}

	osx_output_configure(oo, param);
	oo->mutex = g_mutex_new();
	oo->condition = g_cond_new();

	return &oo->base;
}

static void
osx_output_finish(struct audio_output *ao)
{
	struct osx_output *od = (struct osx_output *)ao;

	g_mutex_free(od->mutex);
	g_cond_free(od->condition);
	g_free(od);
}

static bool
osx_output_set_device(struct osx_output *oo, GError **error)
{
	bool ret = true;
	OSStatus status;
	UInt32 size, numdevices;
	AudioDeviceID *deviceids = NULL;
	char name[256];
	unsigned int i;

	if (oo->component_subtype != kAudioUnitSubType_HALOutput)
		goto done;

	/* how many audio devices are there? */
	status = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices,
					      &size,
					      NULL);
	if (status != noErr) {
		g_set_error(error, osx_output_quark(), status,
			    "Unable to determine number of OS X audio devices: %s",
			    GetMacOSStatusCommentString(status));
		ret = false;
		goto done;
	}

	/* what are the available audio device IDs? */
	numdevices = size / sizeof(AudioDeviceID);
	deviceids = g_malloc(size);
	status = AudioHardwareGetProperty(kAudioHardwarePropertyDevices,
					  &size,
					  deviceids);
	if (status != noErr) {
		g_set_error(error, osx_output_quark(), status,
			    "Unable to determine OS X audio device IDs: %s",
			    GetMacOSStatusCommentString(status));
		ret = false;
		goto done;
	}

	/* which audio device matches oo->device_name? */
	for (i = 0; i < numdevices; i++) {
		size = sizeof(name);
		status = AudioDeviceGetProperty(deviceids[i], 0, false,
						kAudioDevicePropertyDeviceName,
						&size, name);
		if (status != noErr) {
			g_set_error(error, osx_output_quark(), status,
				    "Unable to determine OS X device name "
				    "(device %u): %s",
				    (unsigned int) deviceids[i],
				    GetMacOSStatusCommentString(status));
			ret = false;
			goto done;
		}
		if (strcmp(oo->device_name, name) == 0) {
			g_debug("found matching device: ID=%u, name=%s",
				(unsigned int) deviceids[i], name);
			break;
		}
	}
	if (i == numdevices) {
		g_warning("Found no audio device with name '%s' "
			  "(will use default audio device)",
			  oo->device_name);
		goto done;
	}

	status = AudioUnitSetProperty(oo->au,
				      kAudioOutputUnitProperty_CurrentDevice,
				      kAudioUnitScope_Global,
				      0,
				      &(deviceids[i]),
				      sizeof(AudioDeviceID));
	if (status != noErr) {
		g_set_error(error, osx_output_quark(), status,
			    "Unable to set OS X audio output device: %s",
			    GetMacOSStatusCommentString(status));
		ret = false;
		goto done;
	}
	g_debug("set OS X audio output device ID=%u, name=%s",
		(unsigned int) deviceids[i], name);

done:
	if (deviceids != NULL)
		g_free(deviceids);
	return ret;
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

	assert(od->buffer != NULL);

	g_mutex_lock(od->mutex);

	size_t nbytes;
	const void *src = fifo_buffer_read(od->buffer, &nbytes);

	if (src != NULL) {
		if (nbytes > buffer_size)
			nbytes = buffer_size;

		memcpy(buffer->mData, src, nbytes);
		fifo_buffer_consume(od->buffer, nbytes);
	} else
		nbytes = 0;

	g_cond_signal(od->condition);
	g_mutex_unlock(od->mutex);

	buffer->mDataByteSize = nbytes;

	unsigned i;
	for (i = 1; i < buffer_list->mNumberBuffers; ++i) {
		buffer = &buffer_list->mBuffers[i];
		buffer->mDataByteSize = 0;
	}

	return 0;
}

static bool
osx_output_enable(struct audio_output *ao, GError **error_r)
{
	struct osx_output *oo = (struct osx_output *)ao;

	ComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = oo->component_subtype;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	Component comp = FindNextComponent(NULL, &desc);
	if (comp == 0) {
		g_set_error(error_r, osx_output_quark(), 0,
			    "Error finding OS X component");
		return false;
	}

	OSStatus status = OpenAComponent(comp, &oo->au);
	if (status != noErr) {
		g_set_error(error_r, osx_output_quark(), status,
			    "Unable to open OS X component: %s",
			    GetMacOSStatusCommentString(status));
		return false;
	}

	if (!osx_output_set_device(oo, error_r)) {
		CloseComponent(oo->au);
		return false;
	}

	AURenderCallbackStruct callback;
	callback.inputProc = osx_render;
	callback.inputProcRefCon = oo;

	ComponentResult result =
		AudioUnitSetProperty(oo->au,
				     kAudioUnitProperty_SetRenderCallback,
				     kAudioUnitScope_Input, 0,
				     &callback, sizeof(callback));
	if (result != noErr) {
		CloseComponent(oo->au);
		g_set_error(error_r, osx_output_quark(), result,
			    "unable to set callback for OS X audio unit");
		return false;
	}

	return true;
}

static void
osx_output_disable(struct audio_output *ao)
{
	struct osx_output *oo = (struct osx_output *)ao;

	CloseComponent(oo->au);
}

static void
osx_output_cancel(struct audio_output *ao)
{
	struct osx_output *od = (struct osx_output *)ao;

	g_mutex_lock(od->mutex);
	fifo_buffer_clear(od->buffer);
	g_mutex_unlock(od->mutex);
}

static void
osx_output_close(struct audio_output *ao)
{
	struct osx_output *od = (struct osx_output *)ao;

	AudioOutputUnitStop(od->au);
	AudioUnitUninitialize(od->au);

	fifo_buffer_free(od->buffer);
}

static bool
osx_output_open(struct audio_output *ao, struct audio_format *audio_format, GError **error)
{
	struct osx_output *od = (struct osx_output *)ao;

	AudioStreamBasicDescription stream_description;
	stream_description.mSampleRate = audio_format->sample_rate;
	stream_description.mFormatID = kAudioFormatLinearPCM;
	stream_description.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;

	switch (audio_format->format) {
	case SAMPLE_FORMAT_S8:
		stream_description.mBitsPerChannel = 8;
		break;

	case SAMPLE_FORMAT_S16:
		stream_description.mBitsPerChannel = 16;
		break;

	case SAMPLE_FORMAT_S32:
		stream_description.mBitsPerChannel = 32;
		break;

	default:
		audio_format->format = SAMPLE_FORMAT_S32;
		stream_description.mBitsPerChannel = 32;
		break;
	}

#if G_BYTE_ORDER == G_BIG_ENDIAN
	stream_description.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
#endif

	stream_description.mBytesPerPacket =
		audio_format_frame_size(audio_format);
	stream_description.mFramesPerPacket = 1;
	stream_description.mBytesPerFrame = stream_description.mBytesPerPacket;
	stream_description.mChannelsPerFrame = audio_format->channels;

	ComponentResult result =
		AudioUnitSetProperty(od->au, kAudioUnitProperty_StreamFormat,
				     kAudioUnitScope_Input, 0,
				     &stream_description,
				     sizeof(stream_description));
	if (result != noErr) {
		g_set_error(error, osx_output_quark(), result,
			    "Unable to set format on OS X device");
		return false;
	}

	OSStatus status = AudioUnitInitialize(od->au);
	if (status != noErr) {
		g_set_error(error, osx_output_quark(), status,
			    "Unable to initialize OS X audio unit: %s",
			    GetMacOSStatusCommentString(status));
		return false;
	}

	/* create a buffer of 1s */
	od->buffer = fifo_buffer_new(audio_format->sample_rate *
				     audio_format_frame_size(audio_format));

	status = AudioOutputUnitStart(od->au);
	if (status != 0) {
		AudioUnitUninitialize(od->au);
		g_set_error(error, osx_output_quark(), status,
			    "unable to start audio output: %s",
			    GetMacOSStatusCommentString(status));
		return false;
	}

	return true;
}

static size_t
osx_output_play(struct audio_output *ao, const void *chunk, size_t size,
		G_GNUC_UNUSED GError **error)
{
	struct osx_output *od = (struct osx_output *)ao;

	g_mutex_lock(od->mutex);

	void *dest;
	size_t max_length;

	while (true) {
		dest = fifo_buffer_write(od->buffer, &max_length);
		if (dest != NULL)
			break;

		/* wait for some free space in the buffer */
		g_cond_wait(od->condition, od->mutex);
	}

	if (size > max_length)
		size = max_length;

	memcpy(dest, chunk, size);
	fifo_buffer_append(od->buffer, size);

	g_mutex_unlock(od->mutex);

	return size;
}

const struct audio_output_plugin osx_output_plugin = {
	.name = "osx",
	.test_default_device = osx_output_test_default_device,
	.init = osx_output_init,
	.finish = osx_output_finish,
	.enable = osx_output_enable,
	.disable = osx_output_disable,
	.open = osx_output_open,
	.close = osx_output_close,
	.play = osx_output_play,
	.cancel = osx_output_cancel,
};
