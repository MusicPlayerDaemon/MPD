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
#include <boost/lockfree/spsc_queue.hpp>

struct OSXOutput {
	AudioOutput base;

	/* configuration settings */
	OSType component_subtype;
	/* only applicable with kAudioUnitSubType_HALOutput */
	const char *device_name;
	const char *channel_map;
	bool hog_device;
	bool sync_sample_rate;

	AudioDeviceID dev_id;
	AudioComponentInstance au;
	AudioStreamBasicDescription asbd;

	boost::lockfree::spsc_queue<uint8_t> *ring_buffer;

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
	oo->hog_device = block.GetBlockValue("hog_device", false);
	oo->sync_sample_rate = block.GetBlockValue("sync_sample_rate", false);
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

	AudioObjectPropertyAddress aopa = {
		kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	AudioDeviceID dev_id = kAudioDeviceUnknown;
	UInt32 dev_id_size = sizeof(dev_id);
	AudioObjectGetPropertyData(kAudioObjectSystemObject,
				   &aopa,
				   0,
				   NULL,
				   &dev_id_size,
				   &dev_id);
	oo->dev_id = dev_id;

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

static void
osx_output_sync_device_sample_rate(AudioDeviceID dev_id, AudioStreamBasicDescription desc)
{
	FormatDebug(osx_output_domain, "Syncing sample rate.");
	AudioObjectPropertyAddress aopa = {
		kAudioDevicePropertyAvailableNominalSampleRates,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	UInt32 property_size;
	OSStatus err = AudioObjectGetPropertyDataSize(dev_id,
						      &aopa,
						      0,
						      NULL,
						      &property_size);

	int count = property_size/sizeof(AudioValueRange);
	AudioValueRange ranges[count];
	property_size = sizeof(ranges);
	err = AudioObjectGetPropertyData(dev_id,
					 &aopa,
					 0,
					 NULL,
					 &property_size,
					 &ranges);
	// Get the maximum sample rate as fallback.
	Float64 sample_rate = .0;
	for (int i = 0; i < count; i++) {
		if (ranges[i].mMaximum > sample_rate)
			sample_rate = ranges[i].mMaximum;
	}

	// Now try to see if the device support our format sample rate.
	// For some high quality media samples, the frame rate may exceed
	// device capability. In this case, we let CoreAudio downsample
	// by decimation with an integer factor ranging from 1 to 4.
	for (int f = 4; f > 0; f--) {
		Float64 rate = desc.mSampleRate / f;
		for (int i = 0; i < count; i++) {
			if (ranges[i].mMinimum <= rate
			   && rate <= ranges[i].mMaximum) {
				sample_rate = rate;
				break;
			}
		}
	}

	aopa.mSelector = kAudioDevicePropertyNominalSampleRate,

	err = AudioObjectSetPropertyData(dev_id,
					 &aopa,
					 0,
					 NULL,
					 sizeof(&desc.mSampleRate),
					 &sample_rate);
	if (err != noErr) {
                FormatWarning(osx_output_domain,
			      "Failed to synchronize the sample rate: %d",
			      err);
	} else {
		FormatDebug(osx_output_domain,
			    "Sample rate synced to %f Hz.",
			    sample_rate);
	}
}

static OSStatus
osx_output_set_buffer_size(AudioUnit au, AudioStreamBasicDescription desc, UInt32 *frame_size)
{
	AudioValueRange value_range = {0, 0};
	UInt32 property_size = sizeof(AudioValueRange);
	OSStatus err = AudioUnitGetProperty(au,
					    kAudioDevicePropertyBufferFrameSizeRange,
					    kAudioUnitScope_Global,
					    0,
					    &value_range,
					    &property_size);
	if (err != noErr)
		return err;

	UInt32 buffer_frame_size = value_range.mMaximum;
	err = AudioUnitSetProperty(au,
				   kAudioDevicePropertyBufferFrameSize,
				   kAudioUnitScope_Global,
				   0,
				   &buffer_frame_size,
				   sizeof(buffer_frame_size));
	if (err != noErr)
                FormatWarning(osx_output_domain,
			      "Failed to set maximum buffer size: %d",
			      err);

	property_size = sizeof(buffer_frame_size);
	err = AudioUnitGetProperty(au,
				   kAudioDevicePropertyBufferFrameSize,
				   kAudioUnitScope_Global,
				   0,
				   &buffer_frame_size,
				   &property_size);
	if (err != noErr) {
                FormatWarning(osx_output_domain,
			      "Cannot get the buffer frame size: %d",
			      err);
		return err;
	}

	buffer_frame_size *= desc.mBytesPerFrame;

	// We set the frame size to a power of two integer that
	// is larger than buffer_frame_size.
	while (*frame_size < buffer_frame_size + 1) {
		*frame_size <<= 1;
	}

	return noErr;
}

static void
osx_output_hog_device(AudioDeviceID dev_id, bool hog)
{
	pid_t hog_pid;
	AudioObjectPropertyAddress aopa = {
		kAudioDevicePropertyHogMode,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};
	UInt32 size = sizeof(hog_pid);
	OSStatus err = AudioObjectGetPropertyData(dev_id,
						  &aopa,
						  0,
						  NULL,
						  &size,
						  &hog_pid);
	if (err != noErr) {
		FormatDebug(osx_output_domain,
			    "Cannot get hog information: %d",
			    err);
		return;
	}
	if (hog) {
		if (hog_pid != -1) {
		        FormatDebug(osx_output_domain,
				    "Device is already hogged.");
			return;
		}
	} else {
		if (hog_pid != getpid()) {
		        FormatDebug(osx_output_domain,
				    "Device is not owned by this process.");
			return;
		}
	}
	hog_pid = hog ? getpid() : -1;
	size = sizeof(hog_pid);
	err = AudioObjectSetPropertyData(dev_id,
					 &aopa,
					 0,
					 NULL,
					 size,
					 &hog_pid);
	if (err != noErr) {
		FormatDebug(osx_output_domain,
			    "Cannot hog the device: %d",
			    err);
	} else {
		FormatDebug(osx_output_domain,
			    hog_pid == -1 ? "Device is unhogged" 
					  : "Device is hogged");
	}
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

	oo->dev_id = deviceids[i];
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
*/

static OSStatus
osx_render(void *vdata,
	   gcc_unused AudioUnitRenderActionFlags *io_action_flags,
	   gcc_unused const AudioTimeStamp *in_timestamp,
	   gcc_unused UInt32 in_bus_number,
	   UInt32 in_number_frames,
	   AudioBufferList *buffer_list)
{
	OSXOutput *od = (OSXOutput *) vdata;

	int count = in_number_frames * od->asbd.mBytesPerFrame;
	buffer_list->mBuffers[0].mDataByteSize =
		od->ring_buffer->pop((uint8_t *)buffer_list->mBuffers[0].mData,
				     count);
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

	if (oo->hog_device) {
		osx_output_hog_device(oo->dev_id, true);
	}

	return true;
}

static void
osx_output_disable(AudioOutput *ao)
{
	OSXOutput *oo = (OSXOutput *)ao;

	AudioComponentInstanceDispose(oo->au);

	if (oo->hog_device) {
		osx_output_hog_device(oo->dev_id, false);
	}
}

static void
osx_output_close(AudioOutput *ao)
{
	OSXOutput *od = (OSXOutput *)ao;

	AudioOutputUnitStop(od->au);
	AudioUnitUninitialize(od->au);

	delete od->ring_buffer;
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

	if (od->sync_sample_rate) {
		osx_output_sync_device_sample_rate(od->dev_id, od->asbd);
	}

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

	AURenderCallbackStruct callback;
	callback.inputProc = osx_render;
	callback.inputProcRefCon = od;

	status =
		AudioUnitSetProperty(od->au,
				     kAudioUnitProperty_SetRenderCallback,
				     kAudioUnitScope_Input, 0,
				     &callback, sizeof(callback));
	if (status != noErr) {
		AudioComponentInstanceDispose(od->au);
		error.Set(osx_output_domain, status,
			  "unable to set callback for OS X audio unit");
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

	UInt32 buffer_frame_size = 1;
	status = osx_output_set_buffer_size(od->au, od->asbd, &buffer_frame_size);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		error.Format(osx_output_domain, status,
			     "Unable to set frame size: %s",
			     errormsg);
		return false;
	}

	od->ring_buffer = new boost::lockfree::spsc_queue<uint8_t>(buffer_frame_size);

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
	return od->ring_buffer->push((uint8_t *)chunk, size);
}

static unsigned
osx_output_delay(AudioOutput *ao)
{
	OSXOutput *od = (OSXOutput *)ao;
	return od->ring_buffer->write_available() ? 0 : 25;
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
	osx_output_delay,
	nullptr,
	osx_output_play,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};
