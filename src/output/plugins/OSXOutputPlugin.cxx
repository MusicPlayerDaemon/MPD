/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "OSXOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "util/DynamicFifoBuffer.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "system/ByteOrder.hxx"
#include "Log.hxx"

#include <CoreAudio/AudioHardware.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreServices/CoreServices.h>

struct OSXOutput {
	AudioOutput base;

	/* configuration settings */
	OSType component_subtype;
	/* only applicable with kAudioUnitSubType_HALOutput */
	const char *device_name;

	AudioUnit au;
	Mutex mutex;
	Cond condition;

	DynamicFifoBuffer<uint8_t> *buffer;

	OSXOutput()
		:base(osx_output_plugin) {}
};

static constexpr Domain osx_output_domain("osx_output");

static bool
osx_output_test_default_device(void)
{
	/* on a Mac, this is always the default plugin, if nothing
	   else is configured */
	return true;
}

static void
osx_output_configure(OSXOutput *oo, const config_param &param)
{
	const char *device = param.GetBlockValue("device");

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
		/* XXX am I supposed to strdup() this? */
		oo->device_name = device;
	}
}

static AudioOutput *
osx_output_init(const config_param &param, Error &error)
{
	OSXOutput *oo = new OSXOutput();
	if (!oo->base.Configure(param, error)) {
		delete oo;
		return NULL;
	}

	osx_output_configure(oo, param);

	return &oo->base;
}

static void
osx_output_finish(AudioOutput *ao)
{
	OSXOutput *oo = (OSXOutput *)ao;

	delete oo;
}

static bool
osx_output_set_device(OSXOutput *oo, Error &error)
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
		error.Format(osx_output_domain, status,
			     "Unable to determine number of OS X audio devices: %s",
			     GetMacOSStatusCommentString(status));
		ret = false;
		goto done;
	}

	/* what are the available audio device IDs? */
	numdevices = size / sizeof(AudioDeviceID);
	deviceids = new AudioDeviceID[numdevices];
	status = AudioHardwareGetProperty(kAudioHardwarePropertyDevices,
					  &size,
					  deviceids);
	if (status != noErr) {
		error.Format(osx_output_domain, status,
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
			error.Format(osx_output_domain, status,
				     "Unable to determine OS X device name "
				     "(device %u): %s",
				     (unsigned int) deviceids[i],
				     GetMacOSStatusCommentString(status));
			ret = false;
			goto done;
		}
		if (strcmp(oo->device_name, name) == 0) {
			FormatDebug(osx_output_domain,
				    "found matching device: ID=%u, name=%s",
				    (unsigned)deviceids[i], name);
			break;
		}
	}
	if (i == numdevices) {
		FormatWarning(osx_output_domain,
			      "Found no audio device with name '%s' "
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
		error.Format(osx_output_domain, status,
			     "Unable to set OS X audio output device: %s",
			     GetMacOSStatusCommentString(status));
		ret = false;
		goto done;
	}

	FormatDebug(osx_output_domain,
		    "set OS X audio output device ID=%u, name=%s",
		    (unsigned)deviceids[i], name);

done:
	delete[] deviceids;
	return ret;
}

static OSStatus
osx_render(void *vdata,
	   gcc_unused AudioUnitRenderActionFlags *io_action_flags,
	   gcc_unused const AudioTimeStamp *in_timestamp,
	   gcc_unused UInt32 in_bus_number,
	   gcc_unused UInt32 in_number_frames,
	   AudioBufferList *buffer_list)
{
	OSXOutput *od = (OSXOutput *) vdata;
	AudioBuffer *buffer = &buffer_list->mBuffers[0];
	size_t buffer_size = buffer->mDataByteSize;

	assert(od->buffer != NULL);

	od->mutex.lock();

	auto src = od->buffer->Read();
	if (!src.IsEmpty()) {
		if (src.size > buffer_size)
			src.size = buffer_size;

		memcpy(buffer->mData, src.data, src.size);
		od->buffer->Consume(src.size);
	}

	od->condition.signal();
	od->mutex.unlock();

	buffer->mDataByteSize = src.size;

	unsigned i;
	for (i = 1; i < buffer_list->mNumberBuffers; ++i) {
		buffer = &buffer_list->mBuffers[i];
		buffer->mDataByteSize = 0;
	}

	return 0;
}

static bool
osx_output_enable(AudioOutput *ao, Error &error)
{
	OSXOutput *oo = (OSXOutput *)ao;

	ComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = oo->component_subtype;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	Component comp = FindNextComponent(NULL, &desc);
	if (comp == 0) {
		error.Set(osx_output_domain,
			  "Error finding OS X component");
		return false;
	}

	OSStatus status = OpenAComponent(comp, &oo->au);
	if (status != noErr) {
		error.Format(osx_output_domain, status,
			     "Unable to open OS X component: %s",
			     GetMacOSStatusCommentString(status));
		return false;
	}

	if (!osx_output_set_device(oo, error)) {
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
		error.Set(osx_output_domain, result,
			  "unable to set callback for OS X audio unit");
		return false;
	}

	return true;
}

static void
osx_output_disable(AudioOutput *ao)
{
	OSXOutput *oo = (OSXOutput *)ao;

	CloseComponent(oo->au);
}

static void
osx_output_cancel(AudioOutput *ao)
{
	OSXOutput *od = (OSXOutput *)ao;

	const ScopeLock protect(od->mutex);
	od->buffer->Clear();
}

static void
osx_output_close(AudioOutput *ao)
{
	OSXOutput *od = (OSXOutput *)ao;

	AudioOutputUnitStop(od->au);
	AudioUnitUninitialize(od->au);

	delete od->buffer;
}

static bool
osx_output_open(AudioOutput *ao, AudioFormat &audio_format,
		Error &error)
{
	OSXOutput *od = (OSXOutput *)ao;

	AudioStreamBasicDescription stream_description;
	stream_description.mSampleRate = audio_format.sample_rate;
	stream_description.mFormatID = kAudioFormatLinearPCM;
	stream_description.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;

	switch (audio_format.format) {
	case SampleFormat::S8:
		stream_description.mBitsPerChannel = 8;
		break;

	case SampleFormat::S16:
		stream_description.mBitsPerChannel = 16;
		break;

	case SampleFormat::S32:
		stream_description.mBitsPerChannel = 32;
		break;

	default:
		audio_format.format = SampleFormat::S32;
		stream_description.mBitsPerChannel = 32;
		break;
	}

	if (IsBigEndian())
		stream_description.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;

	stream_description.mBytesPerPacket = audio_format.GetFrameSize();
	stream_description.mFramesPerPacket = 1;
	stream_description.mBytesPerFrame = stream_description.mBytesPerPacket;
	stream_description.mChannelsPerFrame = audio_format.channels;

	ComponentResult result =
		AudioUnitSetProperty(od->au, kAudioUnitProperty_StreamFormat,
				     kAudioUnitScope_Input, 0,
				     &stream_description,
				     sizeof(stream_description));
	if (result != noErr) {
		error.Set(osx_output_domain, result,
			  "Unable to set format on OS X device");
		return false;
	}

	OSStatus status = AudioUnitInitialize(od->au);
	if (status != noErr) {
		error.Format(osx_output_domain, status,
			     "Unable to initialize OS X audio unit: %s",
			     GetMacOSStatusCommentString(status));
		return false;
	}

	/* create a buffer of 1s */
	od->buffer = new DynamicFifoBuffer<uint8_t>(audio_format.sample_rate *
						    audio_format.GetFrameSize());

	status = AudioOutputUnitStart(od->au);
	if (status != 0) {
		AudioUnitUninitialize(od->au);
		error.Format(osx_output_domain, status,
			     "unable to start audio output: %s",
			     GetMacOSStatusCommentString(status));
		return false;
	}

	return true;
}

static size_t
osx_output_play(AudioOutput *ao, const void *chunk, size_t size,
		gcc_unused Error &error)
{
	OSXOutput *od = (OSXOutput *)ao;

	const ScopeLock protect(od->mutex);

	DynamicFifoBuffer<uint8_t>::Range dest;
	while (true) {
		dest = od->buffer->Write();
		if (!dest.IsEmpty())
			break;

		/* wait for some free space in the buffer */
		od->condition.wait(od->mutex);
	}

	if (size > dest.size)
		size = dest.size;

	memcpy(dest.data, chunk, size);
	od->buffer->Append(size);

	return size;
}

const struct AudioOutputPlugin osx_output_plugin = {
	"osx",
	osx_output_test_default_device,
	osx_output_init,
	osx_output_finish,
	osx_output_enable,
	osx_output_disable,
	osx_output_open,
	osx_output_close,
	nullptr,
	nullptr,
	osx_output_play,
	nullptr,
	osx_output_cancel,
	nullptr,
	nullptr,
};
