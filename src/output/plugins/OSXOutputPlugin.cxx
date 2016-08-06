/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "system/ByteOrder.hxx"
#include "Log.hxx"

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreServices/CoreServices.h>

#include<boost/lockfree/spsc_queue.hpp>

struct OSXOutput {
	AudioOutput base;

	/* configuration settings */
	OSType component_subtype;
	/* only applicable with kAudioUnitSubType_HALOutput */
	const char *device_name;
	const char *channel_map;

	AudioComponentInstance au;
	AudioStreamBasicDescription asbd;

	Mutex mutex;
	Cond condition;

	boost::lockfree::spsc_queue<uint8_t> *ring_buffer;
	size_t render_buffer_size;
	uint8_t *render_buffer;

	OSXOutput()
		:base(osx_output_plugin) {}
};

static constexpr Domain osx_output_domain("osx_output");

static void
osx_os_status_to_cstring(OSStatus status, char *str, size_t size) {
	CFErrorRef cferr = CFErrorCreate(nullptr, kCFErrorDomainOSStatus, status, nullptr);
	CFStringRef cfstr = CFErrorCopyDescription(cferr);
	if (!CFStringGetCString(cfstr, str, size, kCFStringEncodingUTF8)) {
		/* conversion failed, return empty string */
		*str = '\0';
	}
	if (cferr)
		CFRelease(cferr);
	if (cfstr)
		CFRelease(cfstr);
}

static bool
osx_output_test_default_device(void)
{
	/* on a Mac, this is always the default plugin, if nothing
	   else is configured */
	return true;
}

static void
osx_output_configure(OSXOutput *oo, const ConfigBlock &block)
{
	const char *device = block.GetBlockValue("device");

	if (device == nullptr || 0 == strcmp(device, "default")) {
		oo->component_subtype = kAudioUnitSubType_DefaultOutput;
		oo->device_name = nullptr;
	}
	else if (0 == strcmp(device, "system")) {
		oo->component_subtype = kAudioUnitSubType_SystemOutput;
		oo->device_name = nullptr;
	}
	else {
		oo->component_subtype = kAudioUnitSubType_HALOutput;
		/* XXX am I supposed to strdup() this? */
		oo->device_name = device;
	}

	oo->channel_map = block.GetBlockValue("channel_map");
}

static AudioOutput *
osx_output_init(const ConfigBlock &block, Error &error)
{
	OSXOutput *oo = new OSXOutput();
	if (!oo->base.Configure(block, error)) {
		delete oo;
		return nullptr;
	}

	osx_output_configure(oo, block);

	return &oo->base;
}

static void
osx_output_finish(AudioOutput *ao)
{
	OSXOutput *oo = (OSXOutput *)ao;

	delete oo;
}

static bool
osx_output_parse_channel_map(
	const char *device_name,
	const char *channel_map_str,
	SInt32 channel_map[],
	UInt32 num_channels,
	Error &error)
{
	char *endptr;
	unsigned int inserted_channels = 0;
	bool want_number = true;

	while (*channel_map_str) {
		if (inserted_channels >= num_channels) {
			error.Format(osx_output_domain,
				"%s: channel map contains more than %u entries or trailing garbage",
				device_name, num_channels);
			return false;
		}

		if (!want_number && *channel_map_str == ',') {
			++channel_map_str;
			want_number = true;
			continue;
		}

		if (want_number &&
			(isdigit(*channel_map_str) || *channel_map_str == '-')
		) {
			channel_map[inserted_channels] = strtol(channel_map_str, &endptr, 10);
			if (channel_map[inserted_channels] < -1) {
				error.Format(osx_output_domain,
					"%s: channel map value %d not allowed (must be -1 or greater)",
					device_name, channel_map[inserted_channels]);
				return false;
			}
			channel_map_str = endptr;
			want_number = false;
			FormatDebug(osx_output_domain,
				"%s: channel_map[%u] = %d",
				device_name, inserted_channels, channel_map[inserted_channels]);
			++inserted_channels;
			continue;
		}

		error.Format(osx_output_domain,
			"%s: invalid character '%c' in channel map",
			device_name, *channel_map_str);
		return false;
	}

	if (inserted_channels < num_channels) {
		error.Format(osx_output_domain,
			"%s: channel map contains less than %u entries",
			device_name, num_channels);
		return false;
	}

	return true;
}

static bool
osx_output_set_channel_map(OSXOutput *oo, Error &error)
{
	AudioStreamBasicDescription desc;
	OSStatus status;
	SInt32 *channel_map = nullptr;
	UInt32 size, num_channels;
	char errormsg[1024];
	bool ret = true;

	size = sizeof(desc);
	memset(&desc, 0, size);
	status = AudioUnitGetProperty(oo->au,
		kAudioUnitProperty_StreamFormat,
		kAudioUnitScope_Output,
		0,
		&desc,
		&size);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		error.Format(osx_output_domain, status,
			"%s: unable to get number of output device channels: %s",
			oo->device_name, errormsg);
		ret = false;
		goto done;
	}

	num_channels = desc.mChannelsPerFrame;
	channel_map = new SInt32[num_channels];
	if (!osx_output_parse_channel_map(oo->device_name,
		oo->channel_map,
		channel_map,
		num_channels,
		error)
	) {
		ret = false;
		goto done;
	}

	size = num_channels * sizeof(SInt32);
	status = AudioUnitSetProperty(oo->au,
		kAudioOutputUnitProperty_ChannelMap,
		kAudioUnitScope_Input,
		0,
		channel_map,
		size);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		error.Format(osx_output_domain, status,
			"%s: unable to set channel map: %s", oo->device_name, errormsg);
		ret = false;
		goto done;
	}

done:
	delete[] channel_map;
	return ret;
}

static bool
osx_output_set_device(OSXOutput *oo, Error &error)
{
	bool ret = true;
	OSStatus status;
	UInt32 size, numdevices;
	AudioDeviceID *deviceids = nullptr;
	AudioObjectPropertyAddress propaddr;
	CFStringRef cfname = nullptr;
	char errormsg[1024];
	char name[256];
	unsigned int i;

	if (oo->component_subtype != kAudioUnitSubType_HALOutput)
		goto done;

	/* how many audio devices are there? */
	propaddr = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
	status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propaddr, 0, nullptr, &size);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		error.Format(osx_output_domain, status,
			     "Unable to determine number of OS X audio devices: %s",
			     errormsg);
		ret = false;
		goto done;
	}

	/* what are the available audio device IDs? */
	numdevices = size / sizeof(AudioDeviceID);
	deviceids = new AudioDeviceID[numdevices];
	status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propaddr, 0, nullptr, &size, deviceids);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		error.Format(osx_output_domain, status,
			     "Unable to determine OS X audio device IDs: %s",
			     errormsg);
		ret = false;
		goto done;
	}

	/* which audio device matches oo->device_name? */
	propaddr = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
	size = sizeof(CFStringRef);
	for (i = 0; i < numdevices; i++) {
		status = AudioObjectGetPropertyData(deviceids[i], &propaddr, 0, nullptr, &size, &cfname);
		if (status != noErr) {
			osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
			error.Format(osx_output_domain, status,
				     "Unable to determine OS X device name "
				     "(device %u): %s",
				     (unsigned int) deviceids[i],
				     errormsg);
			ret = false;
			goto done;
		}

		if (!CFStringGetCString(cfname, name, sizeof(name), kCFStringEncodingUTF8)) {
			error.Set(osx_output_domain, "Unable to convert device name from CFStringRef to char*");
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
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		error.Format(osx_output_domain, status,
			     "Unable to set OS X audio output device: %s",
			     errormsg);
		ret = false;
		goto done;
	}

	FormatDebug(osx_output_domain,
		    "set OS X audio output device ID=%u, name=%s",
		    (unsigned)deviceids[i], name);

	if (oo->channel_map && !osx_output_set_channel_map(oo, error)) {
		ret = false;
		goto done;
	}

done:
	delete[] deviceids;
	if (cfname)
		CFRelease(cfname);
	return ret;
}


/*
	This function (the 'render callback' osx_render) is called by the
	OS X audio subsystem (CoreAudio) to request audio data that will be
	played by the audio hardware. This function has hard time constraints
	so it cannot do IO (debug statements) or memory allocations.
	
	The caller (i.e. CoreAudio) requests a specific number of
	audio frames (in_number_frames) to be rendered into a
	collection of output buffers (buffer_list). Depending on the
	number of output buffers the render callback has to interleave
	or de-interleave audio data to match the layout of the output
	buffers. The intput buffer is always interleaved. In practice,
	it seems this callback always gets a single output buffer
	meaning that no de-interleaving actually takes place. For the
	sake of correctness this callback allows for de-interleaving
	anyway, and calculates the expected output layout by examining
	the output buffers.
*/

static OSStatus
osx_render(void *vdata,
	   gcc_unused AudioUnitRenderActionFlags *io_action_flags,
	   gcc_unused const AudioTimeStamp *in_timestamp,
	   gcc_unused UInt32 in_bus_number,
	   UInt32 in_number_frames,
	   AudioBufferList *buffer_list)
{
	AudioBuffer *output_buffer = nullptr;
	size_t output_buffer_frame_size;

	OSXOutput *od = (OSXOutput *) vdata;

	/*
		By convention when interfacing with audio hardware in CoreAudio,
		in_bus_number equals 0 for output and 1 for input. Because this is an
		audio output plugin our in_bus_number should always be 0.
	*/
	assert(in_bus_number == 0);

	unsigned int input_channel_count = od->asbd.mChannelsPerFrame;
	unsigned int output_channel_count = 0;
	for (unsigned int i = 0 ; i < buffer_list->mNumberBuffers; ++i) {
		output_buffer = &buffer_list->mBuffers[i];
		assert(output_buffer->mData != nullptr);
		output_channel_count += output_buffer->mNumberChannels;
	}
	assert(output_channel_count == input_channel_count);

	size_t input_buffer_frame_size = od->asbd.mBytesPerFrame;
	size_t sample_size = input_buffer_frame_size / input_channel_count;

	size_t requested_bytes = in_number_frames * input_buffer_frame_size;
	if (requested_bytes > od->render_buffer_size)
		requested_bytes = od->render_buffer_size;

	size_t available_bytes = od->ring_buffer->pop(od->render_buffer, requested_bytes);

	/*
		Maybe this is paranoid but we have no way of knowing
		if the 'pop' above ended at a frame boundary. In case
		of an incomplete last frame, keep popping until the
		last frame is complete.
	*/
	size_t remainder;
	while ((remainder = available_bytes % input_buffer_frame_size) > 0)
		available_bytes += od->ring_buffer->pop(
			od->render_buffer + available_bytes,
			input_buffer_frame_size - remainder
		);

	od->condition.signal(); // We are done consuming from ring_buffer

	UInt32 available_frames = available_bytes / input_buffer_frame_size;

	/*
		To de-interleave the data in the input buffer so that it fits in
		the output buffers we divide the input buffer frames into 'sub frames'
		that fit into the output buffers.
	*/
	size_t sub_frame_offset = 0;
	for (unsigned int i = 0 ; i < buffer_list->mNumberBuffers; ++i) {
		output_buffer = &buffer_list->mBuffers[i];
		output_buffer_frame_size = output_buffer->mNumberChannels * sample_size;
		output_buffer->mDataByteSize = 0; // Record how much data we actually rendered
		for (UInt32 current_frame = 0; current_frame < available_frames; ++current_frame) {
				memcpy(
					(uint8_t *) output_buffer->mData + current_frame * output_buffer_frame_size,
					od->render_buffer + current_frame * input_buffer_frame_size + sub_frame_offset,
					output_buffer_frame_size
				);
				output_buffer->mDataByteSize += output_buffer_frame_size;
		}
		sub_frame_offset += output_buffer_frame_size;
	}

	return noErr;
}

static bool
osx_output_enable(AudioOutput *ao, Error &error)
{
	char errormsg[1024];
	OSXOutput *oo = (OSXOutput *)ao;

	AudioComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = oo->component_subtype;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
	if (comp == 0) {
		error.Set(osx_output_domain,
			  "Error finding OS X component");
		return false;
	}

	OSStatus status = AudioComponentInstanceNew(comp, &oo->au);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		error.Format(osx_output_domain, status,
			     "Unable to open OS X component: %s",
			     errormsg);
		return false;
	}

	if (!osx_output_set_device(oo, error)) {
		AudioComponentInstanceDispose(oo->au);
		return false;
	}

	AURenderCallbackStruct callback;
	callback.inputProc = osx_render;
	callback.inputProcRefCon = oo;

	status =
		AudioUnitSetProperty(oo->au,
				     kAudioUnitProperty_SetRenderCallback,
				     kAudioUnitScope_Input, 0,
				     &callback, sizeof(callback));
	if (status != noErr) {
		AudioComponentInstanceDispose(oo->au);
		error.Set(osx_output_domain, status,
			  "unable to set callback for OS X audio unit");
		return false;
	}

	return true;
}

static void
osx_output_disable(AudioOutput *ao)
{
	OSXOutput *oo = (OSXOutput *)ao;

	AudioComponentInstanceDispose(oo->au);
}

static void
osx_output_close(AudioOutput *ao)
{
	OSXOutput *od = (OSXOutput *)ao;

	AudioOutputUnitStop(od->au);
	AudioUnitUninitialize(od->au);

	delete od->ring_buffer;
	delete[] od->render_buffer;
}

static bool
osx_output_open(AudioOutput *ao, AudioFormat &audio_format,
		Error &error)
{
	char errormsg[1024];
	OSXOutput *od = (OSXOutput *)ao;

	memset(&od->asbd, 0, sizeof(od->asbd));
	od->asbd.mSampleRate = audio_format.sample_rate;
	od->asbd.mFormatID = kAudioFormatLinearPCM;
	od->asbd.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;

	switch (audio_format.format) {
	case SampleFormat::S8:
		od->asbd.mBitsPerChannel = 8;
		break;

	case SampleFormat::S16:
		od->asbd.mBitsPerChannel = 16;
		break;

	case SampleFormat::S32:
		od->asbd.mBitsPerChannel = 32;
		break;

	default:
		audio_format.format = SampleFormat::S32;
		od->asbd.mBitsPerChannel = 32;
		break;
	}

	if (IsBigEndian())
		od->asbd.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;

	od->asbd.mBytesPerPacket = audio_format.GetFrameSize();
	od->asbd.mFramesPerPacket = 1;
	od->asbd.mBytesPerFrame = od->asbd.mBytesPerPacket;
	od->asbd.mChannelsPerFrame = audio_format.channels;

	OSStatus status =
		AudioUnitSetProperty(od->au, kAudioUnitProperty_StreamFormat,
				     kAudioUnitScope_Input, 0,
				     &od->asbd,
				     sizeof(od->asbd));
	if (status != noErr) {
		error.Set(osx_output_domain, status,
			  "Unable to set format on OS X device");
		return false;
	}

	status = AudioUnitInitialize(od->au);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		error.Format(osx_output_domain, status,
			     "Unable to initialize OS X audio unit: %s",
			     errormsg);
		return false;
	}

	/* create a ring buffer of 1s */
	od->ring_buffer = new boost::lockfree::spsc_queue<uint8_t>(audio_format.sample_rate * audio_format.GetFrameSize());

	/*
		od->render_buffer_size is the maximum amount of data we
		render in the render callback. Allocate enough space
		for 0.1 s of frames.
	*/
	od->render_buffer_size = (audio_format.sample_rate/10) * audio_format.GetFrameSize();
	od->render_buffer = new uint8_t[od->render_buffer_size];

	status = AudioOutputUnitStart(od->au);
	if (status != 0) {
		AudioUnitUninitialize(od->au);
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		error.Format(osx_output_domain, status,
			     "unable to start audio output: %s",
			     errormsg);
		return false;
	}

	return true;
}

static size_t
osx_output_play(AudioOutput *ao, const void *chunk, size_t size,
		gcc_unused Error &error)
{
	OSXOutput *od = (OSXOutput *)ao;

	while (true) {
		if (od->ring_buffer->write_available() > 0)
			break;

		/* wait for some free space in the buffer */
		od->condition.wait(od->mutex);
	}

	return od->ring_buffer->push((uint8_t *) chunk, size);
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
	nullptr,
	nullptr,
	nullptr,
};
