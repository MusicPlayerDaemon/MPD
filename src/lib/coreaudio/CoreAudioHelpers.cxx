/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
#include "CoreAudioHelpers.hxx"
#include "CoreAudioDevice.hxx"
#include "system/ByteOrder.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringFormat.hxx"
#include "Log.hxx"

static constexpr Domain macos_output_domain("macos_output");

// Helper Functions

AudioDeviceID
FindAudioDevice(const char *search_name) {
	AudioDeviceID device_id = 0;
	
	if (search_name == nullptr)
		throw std::runtime_error("No device name specified.");
	
	if (strncmp(search_name, "default", 7) == 0) {
		device_id = GetDefaultOutputDevice();
		FormatDebug(macos_output_domain, "Returning default device [0x%04x].", (uint)device_id);
		return device_id;
	}
	FormatDebug(macos_output_domain, "Searching for device - %s.", search_name);
	// Obtain a list of all available audio devices
	AudioObjectPropertyAddress aopa;
	aopa.mScope = kAudioObjectPropertyScopeGlobal;
	aopa.mElement = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioHardwarePropertyDevices;
	
	UInt32 size = 0;
	OSStatus ret = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &aopa, 0, NULL, &size);
	if (ret != noErr)
		throw FormatRuntimeError("Unable to retrieve the size of the list of available devices. Error = %s", GetError(ret));
	
	size_t device_count = size / sizeof(AudioDeviceID);
	AudioDeviceID* device_list = new AudioDeviceID[device_count];
	try {
		ret = AudioObjectGetPropertyData(kAudioObjectSystemObject, &aopa, 0, NULL, &size, device_list);
		if (ret != noErr)
			throw FormatRuntimeError("Unable to retrieve the list of available devices. Error = %s", GetError(ret));
		
		// Attempt to locate the requested device
		const char *device_name;
		for (size_t dev = 0; dev < device_count; dev++) {
			CoreAudioDevice device(device_list[dev]);
			device_name = device.GetName();
			if (strcmp(device_name, search_name) == 0) {
				device_id = device_list[dev];
				break;
			}
		}
	}
	catch (...) {
		delete[] device_list;
		throw;
	}
	delete[] device_list;
	if(!device_id) // No device with correct name found
		throw FormatRuntimeError("No CoreAudio device with name %s.", search_name);
	return device_id;
}

AudioDeviceID
GetDefaultOutputDevice() {
	AudioDeviceID device_id = 0;
	
	AudioObjectPropertyAddress  aopa;
	aopa.mScope = kAudioObjectPropertyScopeGlobal;
	aopa.mElement = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
	
	UInt32 size = sizeof(AudioDeviceID);
	OSStatus ret = AudioObjectGetPropertyData(kAudioObjectSystemObject, &aopa, 0, NULL, &size, &device_id);
	
	// Device ID is set to 0 if there is no audio device available
	if (ret != noErr || !device_id)
		throw FormatRuntimeError("Unable to get default output device. Error = %s", GetError(ret));
	
	return device_id;
}


const char *
GetError(OSStatus error) {
	
	/** See https://developer.apple.com/library/archive/samplecode/CoreAudioUtilityClasses/Listings/CoreAudio_PublicUtility_CAXException_h.html
	 *	for the magic that is done here. Basically this is a conversion
	 *	of four one byte chars stored in UInt32 to a string.
	 */
	
	static char error_buffer[16];
	
	UInt32 be_err = CFSwapInt32HostToBig(error);
	char *str = error_buffer;
	memcpy(str + 1, &be_err, 4);
	if (isprint(str[1]) && isprint(str[2]) && isprint(str[3]) && isprint(str[4])) {
		str[0] = str[5] = '\'';
		str[6] = '\0';
	} else if (error > -200000 && error < 200000)
		// no, format it as an integer
		snprintf(str, sizeof(error_buffer), "%d", (int)error);
	else
		snprintf(str, sizeof(error_buffer), "0x%x", (int)error);
	return str;
}

StringBuffer<64>
StreamDescriptionToString(const AudioStreamBasicDescription desc) {
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

AudioBufferList *
AllocateABL(const AudioStreamBasicDescription asbd, const UInt32 capacity_frames) {
	AudioBufferList *buffer_list = nullptr;
	unsigned int num_buffers = (asbd.mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? asbd.mChannelsPerFrame : 1;
	
	buffer_list = (AudioBufferList *)operator new(offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * num_buffers));
	buffer_list->mNumberBuffers = num_buffers;
	for(unsigned int buffer_index = 0; buffer_index < buffer_list->mNumberBuffers; ++buffer_index) {
		try {
			AllocateAudioBuffer(buffer_list->mBuffers[buffer_index], asbd, capacity_frames);
		}
		catch (...) {
			DeallocateABL(buffer_list);
			std::throw_with_nested("Unable to allocate memory for AudioBufferList.");
		}
	}
	return buffer_list;
}

void
DeallocateABL(AudioBufferList *buffer_list) {
	if(buffer_list != nullptr) {
		for(unsigned int buffer_index = 0; buffer_index < buffer_list->mNumberBuffers; ++buffer_index) {
			operator delete(buffer_list->mBuffers[buffer_index].mData);
		}
	}
	operator delete(buffer_list);
}

void
AllocateAudioBuffer(AudioBuffer &buffer, const AudioStreamBasicDescription asbd, const UInt32 capacity_frames) {
	const size_t bytes = asbd.mBytesPerFrame * capacity_frames;
	buffer.mData = operator new(bytes);
	buffer.mDataByteSize = bytes;
	buffer.mNumberChannels = (asbd.mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? 1 : asbd.mChannelsPerFrame;
}

AudioStreamBasicDescription
AudioFormatToASBD(AudioFormat format) {
	
	assert(format.format != SampleFormat::UNDEFINED);
#ifdef ENABLE_DSD
	assert(format.format != SampleFormat::DSD);
#endif
	AudioStreamBasicDescription out_format;
	memset(&out_format, 0, sizeof(out_format));
	out_format.mSampleRate = format.sample_rate;
	out_format.mChannelsPerFrame = format.channels;
	out_format.mFormatID = kAudioFormatLinearPCM;
	out_format.mFramesPerPacket = 1;
	out_format.mBytesPerPacket = out_format.mBytesPerFrame = format.GetFrameSize();
	
	switch (format.format) {
		case SampleFormat::S8:
			out_format.mBitsPerChannel = 8;
			out_format.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			break;
		case SampleFormat::S16:
			out_format.mBitsPerChannel = 16;
			out_format.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			break;
		case SampleFormat::S24_P32:
			out_format.mBitsPerChannel = 24;
			out_format.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			break;
		case SampleFormat::S32:
			out_format.mBitsPerChannel = 32;
			out_format.mFormatFlags = kAudioFormatFlagIsSignedInteger;
			break;
		case SampleFormat::FLOAT:
			out_format.mBitsPerChannel = 32;
			out_format.mFormatFlags = kAudioFormatFlagIsFloat;
			break;
#ifdef ENABLE_DSD
		case SampleFormat::DSD:
#endif
		case SampleFormat::UNDEFINED:
			gcc_unreachable();
			break;
	}
	if(IsBigEndian())
		out_format.mFormatFlags |= kAudioFormatFlagIsBigEndian;
	return out_format;
}

AudioFormat
ASBDToAudioFormat(AudioStreamBasicDescription asbd) {
	assert(asbd.mFormatID == kAudioFormatLinearPCM);

	AudioFormat out_format;
	out_format.sample_rate = asbd.mSampleRate;
	out_format.channels = asbd.mChannelsPerFrame;;
	
	if (asbd.mFormatFlags & kAudioFormatFlagIsFloat)
		out_format.format = SampleFormat::FLOAT;
	else {
		switch (asbd.mBitsPerChannel) {
			case 8:
				out_format.format = SampleFormat::S8;
				break;
			case 16:
				out_format.format = SampleFormat::S16;
				break;
			case 24:
				out_format.format = SampleFormat::S24_P32;
				break;
			case 32:
				out_format.format = SampleFormat::S32;
				break;
		}
	}
	return out_format;
}

void
ParseChannelMap(const char *channel_map_str, std::vector<SInt32> &channel_map) {
	char *endptr;
	bool want_number = true;

	while (*channel_map_str) {
		if (!want_number && *channel_map_str == ',') {
			++channel_map_str;
			want_number = true;
			continue;
		}

		if (want_number && (isdigit(*channel_map_str) || *channel_map_str == '-')) {
			channel_map.push_back((SInt32)strtol(channel_map_str, &endptr, 10));
			if (channel_map.back() < -1)
				throw FormatRuntimeError("Channel map value %d not allowed (must be -1 or greater)", channel_map.back());
			channel_map_str = endptr;
			want_number = false;
			FormatDebug(macos_output_domain, "channel_map[%u] = %d", (UInt32)channel_map.size() - 1, channel_map.back());
			continue;
		}
		throw FormatRuntimeError("Invalid character '%c' in channel map", *channel_map_str);
	}
}
