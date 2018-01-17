/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "mixer/MixerList.hxx"
#include "util/ScopeExit.hxx"
#include "util/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "system/ByteOrder.hxx"
#include "Log.hxx"

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreServices/CoreServices.h>
#include <boost/lockfree/spsc_queue.hpp>

#include <memory>

static constexpr unsigned MPD_OSX_BUFFER_TIME_MS = 100;

struct OSXOutput final : AudioOutput {
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

	OSXOutput(const ConfigBlock &block);

	static AudioOutput *Create(EventLoop &, const ConfigBlock &block);
	int GetVolume();
	void SetVolume(unsigned new_volume);

private:
	void Enable() override;
	void Disable() noexcept override;

	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	std::chrono::steady_clock::duration Delay() const noexcept override;
	size_t Play(const void *chunk, size_t size) override;
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

OSXOutput::OSXOutput(const ConfigBlock &block)
	:AudioOutput(FLAG_ENABLE_DISABLE)
{
	const char *device = block.GetBlockValue("device");

	if (device == nullptr || 0 == strcmp(device, "default")) {
		component_subtype = kAudioUnitSubType_DefaultOutput;
		device_name = nullptr;
	}
	else if (0 == strcmp(device, "system")) {
		component_subtype = kAudioUnitSubType_SystemOutput;
		device_name = nullptr;
	}
	else {
		component_subtype = kAudioUnitSubType_HALOutput;
		/* XXX am I supposed to strdup() this? */
		device_name = device;
	}

	channel_map = block.GetBlockValue("channel_map");
	hog_device = block.GetBlockValue("hog_device", false);
	sync_sample_rate = block.GetBlockValue("sync_sample_rate", false);
}

AudioOutput *
OSXOutput::Create(EventLoop &, const ConfigBlock &block)
{
	OSXOutput *oo = new OSXOutput(block);

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

	return oo;
}


int
OSXOutput::GetVolume()
{
	AudioUnitParameterValue dvolume;
	char errormsg[1024];

	OSStatus status = AudioUnitGetParameter(au, kHALOutputParam_Volume,
			kAudioUnitScope_Global, 0, &dvolume);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError("unable to get volume: %s", errormsg);
	}

	/* see the explanation in SetVolume, below */
	return static_cast<int>(dvolume * dvolume * 100.0);
}

void
OSXOutput::SetVolume(unsigned new_volume) {
	char errormsg[1024];

	/* The scaling below makes shifts in volume greater at the lower end
	 * of the scale. This mimics the "feel" of physical volume levers. This is
	 * generally what users of audio software expect.
	 */

	AudioUnitParameterValue scaled_volume =
		sqrt(static_cast<AudioUnitParameterValue>(new_volume) / 100.0);

	OSStatus status = AudioUnitSetParameter(au, kHALOutputParam_Volume,
			kAudioUnitScope_Global, 0, scaled_volume, 0);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError( "unable to set new volume %u: %s",
				new_volume, errormsg);
	}
}

static void
osx_output_parse_channel_map(
	const char *device_name,
	const char *channel_map_str,
	SInt32 channel_map[],
	UInt32 num_channels)
{
	char *endptr;
	unsigned int inserted_channels = 0;
	bool want_number = true;

	while (*channel_map_str) {
		if (inserted_channels >= num_channels)
			throw FormatRuntimeError("%s: channel map contains more than %u entries or trailing garbage",
						 device_name, num_channels);

		if (!want_number && *channel_map_str == ',') {
			++channel_map_str;
			want_number = true;
			continue;
		}

		if (want_number &&
			(isdigit(*channel_map_str) || *channel_map_str == '-')
		) {
			channel_map[inserted_channels] = strtol(channel_map_str, &endptr, 10);
			if (channel_map[inserted_channels] < -1)
				throw FormatRuntimeError("%s: channel map value %d not allowed (must be -1 or greater)",
							 device_name, channel_map[inserted_channels]);

			channel_map_str = endptr;
			want_number = false;
			FormatDebug(osx_output_domain,
				"%s: channel_map[%u] = %d",
				device_name, inserted_channels, channel_map[inserted_channels]);
			++inserted_channels;
			continue;
		}

		throw FormatRuntimeError("%s: invalid character '%c' in channel map",
					 device_name, *channel_map_str);
	}

	if (inserted_channels < num_channels)
		throw FormatRuntimeError("%s: channel map contains less than %u entries",
					 device_name, num_channels);
}

static void
osx_output_set_channel_map(OSXOutput *oo)
{
	AudioStreamBasicDescription desc;
	OSStatus status;
	UInt32 size, num_channels;
	char errormsg[1024];

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
		throw FormatRuntimeError("%s: unable to get number of output device channels: %s",
					 oo->device_name, errormsg);
	}

	num_channels = desc.mChannelsPerFrame;
	std::unique_ptr<SInt32[]> channel_map(new SInt32[num_channels]);
	osx_output_parse_channel_map(oo->device_name,
				     oo->channel_map,
				     channel_map.get(),
				     num_channels);

	size = num_channels * sizeof(SInt32);
	status = AudioUnitSetProperty(oo->au,
		kAudioOutputUnitProperty_ChannelMap,
		kAudioUnitScope_Input,
		0,
		channel_map.get(),
		size);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError("%s: unable to set channel map: %s", oo->device_name, errormsg);
	}
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


static void
osx_output_set_device(OSXOutput *oo)
{
	OSStatus status;
	UInt32 size, numdevices;
	AudioObjectPropertyAddress propaddr;
	CFStringRef cfname = nullptr;
	char errormsg[1024];
	char name[256];
	unsigned int i;

	AtScopeExit(&cfname) {
		if (cfname)
			CFRelease(cfname);
	};

	if (oo->component_subtype != kAudioUnitSubType_HALOutput)
		return;

	/* how many audio devices are there? */
	propaddr = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
	status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propaddr, 0, nullptr, &size);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError("Unable to determine number of OS X audio devices: %s",
					 errormsg);
	}

	/* what are the available audio device IDs? */
	numdevices = size / sizeof(AudioDeviceID);
	std::unique_ptr<AudioDeviceID[]> deviceids(new AudioDeviceID[numdevices]);
	status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propaddr, 0, nullptr, &size, deviceids.get());
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError("Unable to determine OS X audio device IDs: %s",
					 errormsg);
	}

	/* which audio device matches oo->device_name? */
	propaddr = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
	size = sizeof(CFStringRef);
	for (i = 0; i < numdevices; i++) {
		status = AudioObjectGetPropertyData(deviceids[i], &propaddr, 0, nullptr, &size, &cfname);
		if (status != noErr) {
			osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
			throw FormatRuntimeError("Unable to determine OS X device name "
						 "(device %u): %s",
						 (unsigned int) deviceids[i],
						 errormsg);
		}

		if (!CFStringGetCString(cfname, name, sizeof(name), kCFStringEncodingUTF8))
			throw std::runtime_error("Unable to convert device name from CFStringRef to char*");

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
		return;
	}

	status = AudioUnitSetProperty(oo->au,
				      kAudioOutputUnitProperty_CurrentDevice,
				      kAudioUnitScope_Global,
				      0,
				      &(deviceids[i]),
				      sizeof(AudioDeviceID));
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError("Unable to set OS X audio output device: %s",
					 errormsg);
	}

	oo->dev_id = deviceids[i];
	FormatDebug(osx_output_domain,
		    "set OS X audio output device ID=%u, name=%s",
		    (unsigned)deviceids[i], name);

	if (oo->channel_map)
		osx_output_set_channel_map(oo);
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

void
OSXOutput::Enable()
{
	char errormsg[1024];

	AudioComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = component_subtype;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
	if (comp == 0)
		throw std::runtime_error("Error finding OS X component");

	OSStatus status = AudioComponentInstanceNew(comp, &au);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError("Unable to open OS X component: %s",
					 errormsg);
	}

	try {
		osx_output_set_device(this);
	} catch (...) {
		AudioComponentInstanceDispose(au);
		throw;
	}

	if (hog_device)
		osx_output_hog_device(dev_id, true);
}

void
OSXOutput::Disable() noexcept
{
	AudioComponentInstanceDispose(au);

	if (hog_device)
		osx_output_hog_device(dev_id, false);
}

void
OSXOutput::Close() noexcept
{
	AudioOutputUnitStop(au);
	AudioUnitUninitialize(au);

	delete ring_buffer;
}

void
OSXOutput::Open(AudioFormat &audio_format)
{
	char errormsg[1024];

	memset(&asbd, 0, sizeof(asbd));
	asbd.mSampleRate = audio_format.sample_rate;
	asbd.mFormatID = kAudioFormatLinearPCM;
	asbd.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;

	switch (audio_format.format) {
	case SampleFormat::S8:
		asbd.mBitsPerChannel = 8;
		break;

	case SampleFormat::S16:
		asbd.mBitsPerChannel = 16;
		break;

	case SampleFormat::S32:
		asbd.mBitsPerChannel = 32;
		break;

	default:
		audio_format.format = SampleFormat::S32;
		asbd.mBitsPerChannel = 32;
		break;
	}

	if (IsBigEndian())
		asbd.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;

	asbd.mBytesPerPacket = audio_format.GetFrameSize();
	asbd.mFramesPerPacket = 1;
	asbd.mBytesPerFrame = asbd.mBytesPerPacket;
	asbd.mChannelsPerFrame = audio_format.channels;

	if (sync_sample_rate)
		osx_output_sync_device_sample_rate(dev_id, asbd);

	OSStatus status =
		AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat,
				     kAudioUnitScope_Input, 0,
				     &asbd,
				     sizeof(asbd));
	if (status != noErr)
		throw std::runtime_error("Unable to set format on OS X device");

	AURenderCallbackStruct callback;
	callback.inputProc = osx_render;
	callback.inputProcRefCon = this;

	status =
		AudioUnitSetProperty(au,
				     kAudioUnitProperty_SetRenderCallback,
				     kAudioUnitScope_Input, 0,
				     &callback, sizeof(callback));
	if (status != noErr) {
		AudioComponentInstanceDispose(au);
		throw std::runtime_error("unable to set callback for OS X audio unit");
	}

	status = AudioUnitInitialize(au);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError("Unable to initialize OS X audio unit: %s",
					 errormsg);
	}

	UInt32 buffer_frame_size = 1;
	status = osx_output_set_buffer_size(au, asbd, &buffer_frame_size);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError("Unable to set frame size: %s",
					 errormsg);
	}

	size_t ring_buffer_size = std::max<size_t>(buffer_frame_size,
						   MPD_OSX_BUFFER_TIME_MS * audio_format.GetFrameSize() * audio_format.sample_rate / 1000);
	ring_buffer = new boost::lockfree::spsc_queue<uint8_t>(ring_buffer_size);

	status = AudioOutputUnitStart(au);
	if (status != 0) {
		AudioUnitUninitialize(au);
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError("unable to start audio output: %s",
					 errormsg);
	}
}

size_t
OSXOutput::Play(const void *chunk, size_t size)
{
	return ring_buffer->push((uint8_t *)chunk, size);
}

std::chrono::steady_clock::duration
OSXOutput::Delay() const noexcept
{
	return ring_buffer->write_available()
		? std::chrono::steady_clock::duration::zero()
		: std::chrono::milliseconds(MPD_OSX_BUFFER_TIME_MS / 4);
}

int
osx_output_get_volume(OSXOutput &output)
{
	return output.GetVolume();
}

void
osx_output_set_volume(OSXOutput &output, unsigned new_volume)
{
	return output.SetVolume(new_volume);
}

const struct AudioOutputPlugin osx_output_plugin = {
	"osx",
	osx_output_test_default_device,
	&OSXOutput::Create,
	&osx_mixer_plugin,
};
