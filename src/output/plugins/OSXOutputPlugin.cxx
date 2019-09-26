/*
 * Copyright 2003-2019 The Music Player Daemon Project
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
#include "util/Manual.hxx"
#include "util/ConstBuffer.hxx"
#include "pcm/Export.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "util/ByteOrder.hxx"
#include "util/StringAPI.hxx"
#include "util/StringBuffer.hxx"
#include "util/StringFormat.hxx"
#include "Log.hxx"

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreServices/CoreServices.h>
#include <boost/lockfree/spsc_queue.hpp>

#include <memory>

static constexpr unsigned MPD_OSX_BUFFER_TIME_MS = 100;

static StringBuffer<64>
StreamDescriptionToString(const AudioStreamBasicDescription desc)
{
	// Only convert the lpcm formats (nothing else supported / used by MPD)
	assert(desc.mFormatID == kAudioFormatLinearPCM);

	return StringFormat<64>("%u channel %s %sinterleaved %u-bit %s %s (%uHz)",
				desc.mChannelsPerFrame,
				(desc.mFormatFlags & kAudioFormatFlagIsNonMixable) ? "" : "mixable",
				(desc.mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? "non-" : "",
				desc.mBitsPerChannel,
				(desc.mFormatFlags & kAudioFormatFlagIsFloat) ? "Float" : "SInt",
				(desc.mFormatFlags & kAudioFormatFlagIsBigEndian) ? "BE" : "LE",
				(UInt32)desc.mSampleRate);
}


struct OSXOutput final : AudioOutput {
	/* configuration settings */
	OSType component_subtype;
	/* only applicable with kAudioUnitSubType_HALOutput */
	const char *device_name;
	const char *channel_map;
	bool hog_device;
	bool pause;
#ifdef ENABLE_DSD
	/**
	 * Enable DSD over PCM according to the DoP standard?
	 *
	 * @see http://dsd-guide.com/dop-open-standard
	 */
	bool dop_setting;
	bool dop_enabled;
	Manual<PcmExport> pcm_export;
#endif

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
	bool Pause() override;
	void Cancel() noexcept override;
};

static constexpr Domain osx_output_domain("osx_output");

static void
osx_os_status_to_cstring(OSStatus status, char *str, size_t size)
{
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
osx_output_test_default_device()
{
	/* on a Mac, this is always the default plugin, if nothing
	   else is configured */
	return true;
}

OSXOutput::OSXOutput(const ConfigBlock &block)
	:AudioOutput(FLAG_ENABLE_DISABLE|FLAG_PAUSE)
{
	const char *device = block.GetBlockValue("device");

	if (device == nullptr || StringIsEqual(device, "default")) {
		component_subtype = kAudioUnitSubType_DefaultOutput;
		device_name = nullptr;
	}
	else if (StringIsEqual(device, "system")) {
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
#ifdef ENABLE_DSD
	dop_setting = block.GetBlockValue("dop", false);
#endif
}

AudioOutput *
OSXOutput::Create(EventLoop &, const ConfigBlock &block)
{
	OSXOutput *oo = new OSXOutput(block);
	AudioObjectPropertyAddress aopa;
	AudioDeviceID dev_id = kAudioDeviceUnknown;
	UInt32 dev_id_size = sizeof(dev_id);

	if (oo->component_subtype == kAudioUnitSubType_SystemOutput)
		// get system output dev_id if configured
		aopa = {
			kAudioHardwarePropertyDefaultSystemOutputDevice,
			kAudioObjectPropertyScopeOutput,
			kAudioObjectPropertyElementMaster
		};
	else
		/* fallback to default device initially (can still be
		   changed by osx_output_set_device) */
		aopa = {
			kAudioHardwarePropertyDefaultOutputDevice,
			kAudioObjectPropertyScopeOutput,
			kAudioObjectPropertyElementMaster
		};

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
	Float32 vol;
	AudioObjectPropertyAddress aopa = {
		.mSelector	= kAudioHardwareServiceDeviceProperty_VirtualMasterVolume,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster,
	};
	UInt32 size = sizeof(vol);
	OSStatus status = AudioObjectGetPropertyData(dev_id,
						     &aopa,
						     0,
						     NULL,
						     &size,
						     &vol);

	if (status != noErr) {
		char errormsg[1024];
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError("unable to get volume: %s", errormsg);
	}

	return static_cast<int>(vol * 100.0);
}

void
OSXOutput::SetVolume(unsigned new_volume)
{
	Float32 vol = new_volume / 100.0;
	AudioObjectPropertyAddress aopa = {
		.mSelector	= kAudioHardwareServiceDeviceProperty_VirtualMasterVolume,
		.mScope		= kAudioObjectPropertyScopeOutput,
		.mElement	= kAudioObjectPropertyElementMaster
	};
	UInt32 size = sizeof(vol);
	OSStatus status = AudioObjectSetPropertyData(dev_id,
						     &aopa,
						     0,
						     NULL,
						     size,
						     &vol);

	if (status != noErr) {
		char errormsg[1024];
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError( "unable to set new volume %u: %s",
				new_volume, errormsg);
	}
}

static void
osx_output_parse_channel_map(const char *device_name,
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


static float
osx_output_score_sample_rate(Float64 destination_rate, unsigned source_rate)
{
	float score = 0;
	double int_portion;
	double frac_portion = modf(source_rate / destination_rate, &int_portion);
	// prefer sample rates that are multiples of the source sample rate
	score += (1 - frac_portion) * 1000;
	// prefer exact matches over other multiples
	score += (int_portion == 1.0) ? 500 : 0;
	if (source_rate == destination_rate)
		score += 1000;
	else if (source_rate > destination_rate)
		score += (int_portion > 1 && int_portion < 100) ? (100 - int_portion) / 100 * 100 : 0;
	else
		score += (int_portion > 1 && int_portion < 100) ? (100 + int_portion) / 100 * 100 : 0;

	return score;
}

static float
osx_output_score_format(const AudioStreamBasicDescription &format_desc,
			const AudioStreamBasicDescription &target_format)
{
	float score = 0;
	// Score only linear PCM formats (everything else MPD cannot use)
	if (format_desc.mFormatID == kAudioFormatLinearPCM) {
		score += osx_output_score_sample_rate(format_desc.mSampleRate,
						      target_format.mSampleRate);

		// Just choose the stream / format with the highest number of output channels
		score += format_desc.mChannelsPerFrame * 5;

		if (target_format.mFormatFlags == kLinearPCMFormatFlagIsFloat) {
			// for float, prefer the highest bitdepth we have
			if (format_desc.mBitsPerChannel >= 16)
				score += (format_desc.mBitsPerChannel / 8);
		} else {
			if (format_desc.mBitsPerChannel == target_format.mBitsPerChannel)
				score += 5;
			else if (format_desc.mBitsPerChannel > target_format.mBitsPerChannel)
				score += 1;

		}
	}

	return score;
}

static Float64
osx_output_set_device_format(AudioDeviceID dev_id,
			     const AudioStreamBasicDescription &target_format)
{
	AudioObjectPropertyAddress aopa = {
		kAudioDevicePropertyStreams,
		kAudioObjectPropertyScopeOutput,
		kAudioObjectPropertyElementMaster
	};

	UInt32 property_size;
	OSStatus err = AudioObjectGetPropertyDataSize(dev_id, &aopa, 0, NULL,
						      &property_size);
	if (err != noErr)
		throw FormatRuntimeError("Cannot get number of streams: %d", err);

	const size_t n_streams = property_size / sizeof(AudioStreamID);
	static constexpr size_t MAX_STREAMS = 64;
	if (n_streams > MAX_STREAMS)
		throw std::runtime_error("Too many streams");

	AudioStreamID streams[MAX_STREAMS];
	err = AudioObjectGetPropertyData(dev_id, &aopa, 0, NULL,
					 &property_size, streams);
	if (err != noErr)
		throw FormatRuntimeError("Cannot get streams: %d", err);

	bool format_found = false;
	int output_stream;
	AudioStreamBasicDescription output_format;

	for (size_t i = 0; i < n_streams; i++) {
		UInt32 direction;
		AudioStreamID stream = streams[i];
		aopa.mSelector = kAudioStreamPropertyDirection;
		property_size = sizeof(direction);
		err = AudioObjectGetPropertyData(stream,
						 &aopa,
						 0,
						 NULL,
						 &property_size,
						 &direction);
		if (err != noErr)
			throw FormatRuntimeError("Cannot get streams direction: %d",
						 err);

		if (direction != 0)
			continue;

		aopa.mSelector = kAudioStreamPropertyAvailablePhysicalFormats;
		err = AudioObjectGetPropertyDataSize(stream, &aopa, 0, NULL,
						     &property_size);
		if (err != noErr)
			throw FormatRuntimeError("Unable to get format size s for stream %d. Error = %s",
						 streams[i], err);

		const size_t format_count = property_size / sizeof(AudioStreamRangedDescription);
		static constexpr size_t MAX_FORMATS = 256;
		if (format_count > MAX_FORMATS)
			throw std::runtime_error("Too many formats");

		AudioStreamRangedDescription format_list[MAX_FORMATS];
		err = AudioObjectGetPropertyData(stream, &aopa, 0, NULL,
						 &property_size, format_list);
		if (err != noErr)
			throw FormatRuntimeError("Unable to get available formats for stream %d. Error = %s",
						 streams[i], err);

		float output_score = 0;
		for (size_t j = 0; j < format_count; j++) {
			AudioStreamBasicDescription format_desc = format_list[j].mFormat;
			std::string format_string;

			// for devices with kAudioStreamAnyRate
			// we use the requested samplerate here
			if (format_desc.mSampleRate == kAudioStreamAnyRate)
				format_desc.mSampleRate = target_format.mSampleRate;
			float score = osx_output_score_format(format_desc, target_format);

			// print all (linear pcm) formats and their rating
			if (score > 0.0)
				FormatDebug(osx_output_domain,
					    "Format: %s rated %f",
					    StreamDescriptionToString(format_desc).c_str(), score);

			if (score > output_score) {
				output_score  = score;
				output_format = format_desc;
				output_stream = stream; // set the idx of the stream in the device
				format_found = true;
			}
		}
	}

	if (format_found) {
		aopa.mSelector = kAudioStreamPropertyPhysicalFormat;
		err = AudioObjectSetPropertyData(output_stream,
						 &aopa,
						 0,
						 NULL,
						 sizeof(output_format),
						 &output_format);
		if (err != noErr)
			throw FormatRuntimeError("Failed to change the stream format: %d",
						 err);
	}

	return output_format.mSampleRate;
}

static OSStatus
osx_output_set_buffer_size(AudioUnit au, AudioStreamBasicDescription desc,
			   UInt32 *frame_size)
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
		LogDebug(osx_output_domain,
			 hog_pid == -1
			 ? "Device is unhogged"
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
	propaddr = { kAudioHardwarePropertyDevices,
		     kAudioObjectPropertyScopeGlobal,
		     kAudioObjectPropertyElementMaster };
	status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
						&propaddr, 0, nullptr, &size);
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError("Unable to determine number of OS X audio devices: %s",
					 errormsg);
	}

	/* what are the available audio device IDs? */
	numdevices = size / sizeof(AudioDeviceID);
	std::unique_ptr<AudioDeviceID[]> deviceids(new AudioDeviceID[numdevices]);
	status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
					    &propaddr, 0, nullptr,
					    &size, deviceids.get());
	if (status != noErr) {
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError("Unable to determine OS X audio device IDs: %s",
					 errormsg);
	}

	/* which audio device matches oo->device_name? */
	propaddr = { kAudioObjectPropertyName,
		     kAudioObjectPropertyScopeGlobal,
		     kAudioObjectPropertyElementMaster };
	size = sizeof(CFStringRef);
	for (i = 0; i < numdevices; i++) {
		status = AudioObjectGetPropertyData(deviceids[i], &propaddr,
						    0, nullptr,
						    &size, &cfname);
		if (status != noErr) {
			osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
			throw FormatRuntimeError("Unable to determine OS X device name "
						 "(device %u): %s",
						 (unsigned int) deviceids[i],
						 errormsg);
		}

		if (!CFStringGetCString(cfname, name, sizeof(name),
					kCFStringEncodingUTF8))
			throw std::runtime_error("Unable to convert device name from CFStringRef to char*");

		if (StringIsEqual(oo->device_name, name)) {
			FormatDebug(osx_output_domain,
				    "found matching device: ID=%u, name=%s",
				    (unsigned)deviceids[i], name);
			break;
		}
	}

	if (i == numdevices)
		throw FormatRuntimeError("Found no audio device with name '%s' ",
			      oo->device_name);

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


/**
 * This function (the 'render callback' osx_render) is called by the
 * OS X audio subsystem (CoreAudio) to request audio data that will be
 * played by the audio hardware. This function has hard time
 * constraints so it cannot do IO (debug statements) or memory
 * allocations.
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
#ifdef ENABLE_DSD
	pcm_export.Construct();
#endif

	try {
		osx_output_set_device(this);
	} catch (...) {
		AudioComponentInstanceDispose(au);
#ifdef ENABLE_DSD
		pcm_export.Destruct();
#endif
		throw;
	}

	if (hog_device)
		osx_output_hog_device(dev_id, true);
}

void
OSXOutput::Disable() noexcept
{
	AudioComponentInstanceDispose(au);
#ifdef ENABLE_DSD
	pcm_export.Destruct();
#endif

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
#ifdef ENABLE_DSD
	PcmExport::Params params;
	params.alsa_channel_order = true;
	bool dop = dop_setting;
#endif

	memset(&asbd, 0, sizeof(asbd));
	asbd.mFormatID = kAudioFormatLinearPCM;
	if (audio_format.format == SampleFormat::FLOAT) {
		asbd.mFormatFlags = kLinearPCMFormatFlagIsFloat;
	} else {
		asbd.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
	}

	if (IsBigEndian())
		asbd.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;

	if (audio_format.format == SampleFormat::S24_P32) {
		asbd.mBitsPerChannel = 24;
	} else {
		asbd.mBitsPerChannel = audio_format.GetSampleSize() * 8;
	}
	asbd.mBytesPerPacket = audio_format.GetFrameSize();
	asbd.mSampleRate = audio_format.sample_rate;

#ifdef ENABLE_DSD
	if (dop && audio_format.format == SampleFormat::DSD) {
		asbd.mBitsPerChannel = 24;
		params.dsd_mode = PcmExport::DsdMode::DOP;
		asbd.mSampleRate = params.CalcOutputSampleRate(audio_format.sample_rate);
		asbd.mBytesPerPacket = 4 * audio_format.channels;

	}
#endif

	asbd.mFramesPerPacket = 1;
	asbd.mBytesPerFrame = asbd.mBytesPerPacket;
	asbd.mChannelsPerFrame = audio_format.channels;

	Float64 sample_rate = osx_output_set_device_format(dev_id, asbd);

#ifdef ENABLE_DSD
	if (audio_format.format == SampleFormat::DSD &&
	    sample_rate != asbd.mSampleRate) {
		// fall back to PCM in case sample_rate cannot be synchronized
		params.dsd_mode = PcmExport::DsdMode::NONE;
		audio_format.format = SampleFormat::S32;
		asbd.mBitsPerChannel = 32;
		asbd.mBytesPerPacket = audio_format.GetFrameSize();
		asbd.mSampleRate = params.CalcOutputSampleRate(audio_format.sample_rate);
		asbd.mBytesPerFrame = asbd.mBytesPerPacket;
	}
	dop_enabled = params.dsd_mode == PcmExport::DsdMode::DOP;
#endif

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
		throw std::runtime_error("Unable to set callback for OS X audio unit");
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

#ifdef ENABLE_DSD
	if (dop_enabled) {
		pcm_export->Open(audio_format.format, audio_format.channels, params);
		ring_buffer_size = std::max<size_t>(buffer_frame_size,
						   MPD_OSX_BUFFER_TIME_MS * pcm_export->GetOutputFrameSize() * asbd.mSampleRate / 1000);
	}
#endif
	ring_buffer = new boost::lockfree::spsc_queue<uint8_t>(ring_buffer_size);

	status = AudioOutputUnitStart(au);
	if (status != 0) {
		AudioUnitUninitialize(au);
		osx_os_status_to_cstring(status, errormsg, sizeof(errormsg));
		throw FormatRuntimeError("Unable to start audio output: %s",
					 errormsg);
	}
	pause = false;
}

size_t
OSXOutput::Play(const void *chunk, size_t size)
{
	assert(size > 0);
	if (pause) {
		pause = false;
		OSStatus status = AudioOutputUnitStart(au);
		if (status != 0) {
			AudioUnitUninitialize(au);
			throw std::runtime_error("Unable to restart audio output after pause");
		}
	}
#ifdef ENABLE_DSD
	if (dop_enabled) {
		const auto e = pcm_export->Export({chunk, size});
		if (e.empty())
			return size;

		size_t bytes_written = ring_buffer->push((const uint8_t *)e.data, e.size);
		return pcm_export->CalcInputSize(bytes_written);
	}
#endif
	return ring_buffer->push((const uint8_t *)chunk, size);
}

std::chrono::steady_clock::duration
OSXOutput::Delay() const noexcept
{
	return ring_buffer->write_available() && !pause
		? std::chrono::steady_clock::duration::zero()
		: std::chrono::milliseconds(MPD_OSX_BUFFER_TIME_MS / 4);
}

bool OSXOutput::Pause()
{
	if (!pause) {
		pause = true;
		AudioOutputUnitStop(au);
	}
	return true;
}

void
OSXOutput::Cancel() noexcept
{
	AudioOutputUnitStop(au);
	ring_buffer->reset();
#ifdef ENABLE_DSD
	pcm_export->Reset();
#endif
	AudioOutputUnitStart(au);
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
