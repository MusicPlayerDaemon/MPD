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

#include "CoreAudioDevice.hxx"
#include "CoreAudioHelpers.hxx"
#include "CoreAudioHardware.hxx"
#include "util/StringBuffer.hxx"
#include <AudioToolbox/AudioToolbox.h>
#include "system/ByteOrder.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

static constexpr Domain macos_output_domain("macos_output");

CoreAudioDevice::CoreAudioDevice()  :
	started					(false	),
	device_id				(0		),
	io_proc					(nullptr),
	has_volume				(false	),
	hog_pid					(-1		),
	buffer_size_restore		(0		)
{
	output_format.mFormatID = 0;
}

CoreAudioDevice::CoreAudioDevice(AudioDeviceID dev_id) :
	started					(false		),
	device_id				(dev_id		),
	io_proc					(nullptr	),
	has_volume				(false		),
	hog_pid					(-1			),
	buffer_size_restore		(0			)
{
	output_format.mFormatID = 0;
}

CoreAudioDevice::~CoreAudioDevice()
{
	Close();
}

bool
CoreAudioDevice::Open(const std::string &device_name)
{
	if(!(device_id = CCoreAudioHardware::FindAudioDevice(device_name)))
		return false;
	
	if(!Enumerate())
	{
		FormatError(macos_output_domain, "Failed to get stream information from output device.");
		return false;
	}

	AudioObjectPropertyAddress aopa;
	aopa.mScope		= kAudioDevicePropertyScopeOutput;
	aopa.mElement	= 0;
	aopa.mSelector	= kAudioHardwareServiceDeviceProperty_VirtualMasterVolume;
	Boolean writable;
	
	if(AudioObjectHasProperty(device_id, &aopa))
	{
		OSStatus err = AudioObjectIsPropertySettable(device_id, &aopa, &writable);
		if (err != noErr)
		{
			FormatError(macos_output_domain, "Unable to get propertyinfo volume support. Error = %s", GetError(err).c_str());
			has_volume = false;
		}
		else
			has_volume = (bool)writable;
	}
	else
	{
		has_volume = false;
		FormatInfo(macos_output_domain, "The audio device (id 0x%04x) does not have Volume property.", (uint)device_id);
	}
	return true;
}

bool
CoreAudioDevice::Enumerate()
{
	AudioStreamIdList stream_list;
	bool ret = false;
	stream_infos.clear();
	is_planar = true;
	
	if (GetStreams(&stream_list))
	{
		for (UInt32 stream_idx = 0; stream_idx < stream_list.size(); stream_idx++)
		{
			stream_info info;
			info.stream_id = stream_list[stream_idx];
			info.num_channels = GetNumChannelsOfStream(stream_idx);
			// one stream with num channels other than 1 is enough to make this device non-planar
			if (info.num_channels != 1)
				is_planar = false;
			CoreAudioStream::GetAvailablePhysicalFormats(stream_list[stream_idx], &info.format_list);
			stream_infos.push_back(info);
		}
		ret = true;
	}
	return ret;
}

void
CoreAudioDevice::Close()
{
	if (!device_id)
		return;

	// Stop the device if it was started
	Stop();

	// Unregister the IOProc if available
	RemoveIOProc();

	// Make sure to release hog state
	SetHogStatus(false);

	if (buffer_size_restore)
	{
		SetBufferSize(buffer_size_restore);
		buffer_size_restore = 0;
	}
	io_proc = nullptr;
	device_id = 0;
}

void
CoreAudioDevice::Start()
{
	if (!device_id || started)
		return;

	OSStatus err = AudioDeviceStart(device_id, io_proc);
	if (err != noErr)
		FormatError(macos_output_domain, "Unable to start device. Error = %s", GetError(err).c_str());
	else
		started = true;
}

void
CoreAudioDevice::Stop()
{
	if (!device_id || !started)
		return;

	OSStatus err = AudioDeviceStop(device_id, io_proc);
	if (err != noErr)
		FormatError(macos_output_domain, "Unable to stop device. Error = %s", GetError(err).c_str());
	started = false;
	
}

bool
CoreAudioDevice::AddIOProc(AudioDeviceIOProc callback_function, void* callback_data)
{
	assert(output_format.mFormatID != 0);
	
	// Allow only one IOProc at a time
	if (!device_id || io_proc)
		return false;

	// Open the output stream and make the desired format active
	output_stream.Open(stream_infos[output_stream_idx].stream_id);
	if (output_format.mFormatFlags & kAudioFormatFlagIsNonMixable)
		output_stream.SetVirtualFormat(&output_format);
	output_stream.SetPhysicalFormat(&output_format);

	// Create the callback
	OSStatus err = AudioDeviceCreateIOProcID(device_id, callback_function, callback_data, &io_proc);
	if (err != noErr)
	{
		FormatError(macos_output_domain, "Unable to add IOProc. Error = %s", GetError(err).c_str());
		io_proc = nullptr;
		return false;
	}
	// Start the device (callback)
	Start();
	return true;
}

bool
CoreAudioDevice::RemoveIOProc()
{
	if (!device_id || !io_proc)
		return false;
	
	Stop();
	
	// Close the output stream again and reverse format changes
	output_stream.Close();

	OSStatus err = AudioDeviceDestroyIOProcID(device_id, io_proc);
	if (err != noErr)
		FormatError(macos_output_domain, "Unable to remove IOProc. Error = %s", GetError(err).c_str());

	io_proc = nullptr; // Clear the reference no matter what
	
	return true;
}

std::string
CoreAudioDevice::GetName() const
{
	if (!device_id)
		return "";

	AudioObjectPropertyAddress  aopa;
	aopa.mScope    = kAudioDevicePropertyScopeOutput;
	aopa.mElement  = 0;
	aopa.mSelector = kAudioDevicePropertyDeviceNameCFString;

	std::string name;
	CFStringRef deviceName = nullptr;
	UInt32 propertySize = sizeof(deviceName);

	OSStatus err = AudioObjectGetPropertyData(device_id, &aopa, 0, NULL, &propertySize, &deviceName);

	if (err != noErr)
	{
		FormatError(macos_output_domain, "Unable to get device name - id: 0x%04x. Error = %s", (uint)device_id, GetError(err).c_str());
	}
	else
	{
		// The +1 is for having space for the string to be NUL terminated
		CFIndex bufferSize = CFStringGetLength(deviceName) + 1;
		char buffer[bufferSize];
		
		if (CFStringGetCString(deviceName, buffer, bufferSize, kCFStringEncodingUTF8))
		{
			name.assign(buffer);
		}
		CFRelease(deviceName);
	}
	return name;
}

UInt32
CoreAudioDevice::GetTotalOutputChannels() const
{
	UInt32 channels = 0;

	if (!device_id)
		return channels;

	AudioObjectPropertyAddress  aopa;
	aopa.mScope    = kAudioDevicePropertyScopeOutput;
	aopa.mElement  = 0;
	aopa.mSelector = kAudioDevicePropertyStreamConfiguration;

	UInt32 size = 0;
	OSStatus err = AudioObjectGetPropertyDataSize(device_id, &aopa, 0, NULL, &size);
	if (err != noErr)
		return channels;

	AudioBufferList* buffer_list = (AudioBufferList*)malloc(size);
	err = AudioObjectGetPropertyData(device_id, &aopa, 0, NULL, &size, buffer_list);
	if (err == noErr)
	{
		for(UInt32 buffer = 0; buffer < buffer_list->mNumberBuffers; ++buffer)
			channels += buffer_list->mBuffers[buffer].mNumberChannels;
	}
	else
	{
		FormatError(macos_output_domain, "Unable to get total device output channels - id: 0x%04x. Error = %s", (uint)device_id, GetError(err).c_str());
	}

	free(buffer_list);

	return channels;
}

UInt32
CoreAudioDevice::GetNumChannelsOfStream(UInt32 stream_idx) const
{
	UInt32 channels = 0;

	if (!device_id)
		return channels;
  
	AudioObjectPropertyAddress  aopa;
	aopa.mScope    = kAudioDevicePropertyScopeOutput;
	aopa.mElement  = 0;
	aopa.mSelector = kAudioDevicePropertyStreamConfiguration;

	UInt32 size = 0;
	OSStatus err = AudioObjectGetPropertyDataSize(device_id, &aopa, 0, NULL, &size);
	if (err != noErr)
		return channels;
  
	AudioBufferList* buffer_list = (AudioBufferList*)malloc(size);
	err = AudioObjectGetPropertyData(device_id, &aopa, 0, NULL, &size, buffer_list);
	if (err == noErr)
	{
		if (stream_idx < buffer_list->mNumberBuffers)
			channels = buffer_list->mBuffers[stream_idx].mNumberChannels;
	}
	else
	{
	  FormatError(macos_output_domain, "Unable to get number of stream output channels - id: 0x%04x. Error = %s.",
				  (uint)device_id, GetError(err).c_str());
	}
  
	free(buffer_list);
  
	return channels;
}

bool
CoreAudioDevice::GetStreams(AudioStreamIdList* stream_id_list)
{
	if (!stream_id_list || !device_id)
		return false;

	// Get only output streams (mScope)
	AudioObjectPropertyAddress  aopa;
	aopa.mScope    = kAudioDevicePropertyScopeOutput;
	aopa.mElement  = 0;
	aopa.mSelector = kAudioDevicePropertyStreams;

	UInt32  size = 0;
	OSStatus err = AudioObjectGetPropertyDataSize(device_id, &aopa, 0, NULL, &size);
	if (err != noErr)
		return false;

	UInt32 stream_count = size / sizeof(AudioStreamID);
	AudioStreamID* streams_list = new AudioStreamID[stream_count];
	
	err = AudioObjectGetPropertyData(device_id, &aopa, 0, NULL, &size, streams_list);
	if (err == noErr)
	{
		for (UInt32 stream = 0; stream < stream_count; stream++)
			stream_id_list->push_back(streams_list[stream]);
	}
	
	delete[] streams_list;
	return err == noErr;
}

bool
CoreAudioDevice::SetHogStatus(bool hog)
{
  /* According to Jeff Moore (Core Audio, Apple), Setting kAudioDevicePropertyHogMode
   is a toggle and the only way to tell if you do get hog mode is to compare
   the returned pid against getpid, if the match, you have hog mode, if not you don't. */
	if (!device_id)
		return false;

	AudioObjectPropertyAddress  aopa;
	aopa.mScope    = kAudioDevicePropertyScopeOutput;
	aopa.mElement  = 0;
	aopa.mSelector = kAudioDevicePropertyHogMode;

	if (hog)
	{
		// Not already set
		if (hog_pid == -1)
		{
			OSStatus err = AudioObjectSetPropertyData(device_id, &aopa, 0, NULL, sizeof(hog_pid), &hog_pid);

			/* Even if setting hogmode was successful our PID might not get written
			 into hog_pid (so it stays -1). Readback hogstatus for judging if we
			 had success on getting hog status
			 We do this only when AudioObjectSetPropertyData didn't set hog_pid because
			 it seems that in the other cases the GetHogStatus could return -1
			 which would overwrite our valid hog_pid again */
			if (hog_pid == -1)
				hog_pid = GetHogStatus();

			if (err || hog_pid != getpid())
			{
				FormatError(macos_output_domain, "Unable to set hog mode. Error = %s", GetError(err).c_str());
				return false;
			}
		}
	}
	else
	{
		// Currently Set
		if (hog_pid > -1)
		{
			pid_t unhog_pid = -1;
			OSStatus err = AudioObjectSetPropertyData(device_id, &aopa, 0, NULL, sizeof(unhog_pid), &unhog_pid);
			if (err || unhog_pid == getpid())
			{
				FormatError(macos_output_domain, "Unable to release hog mode. Error = %s", GetError(err).c_str());
				return false;
			}
			// Reset internal state
			hog_pid = unhog_pid;
		}
	}
	return true;
}

pid_t
CoreAudioDevice::GetHogStatus()
{
	if (!device_id)
		return false;

	AudioObjectPropertyAddress  aopa;
	aopa.mScope    = kAudioDevicePropertyScopeOutput;
	aopa.mElement  = 0;
	aopa.mSelector = kAudioDevicePropertyHogMode;

	pid_t hogPid = -1;
	UInt32 size = sizeof(hogPid);
	AudioObjectGetPropertyData(device_id, &aopa, 0, NULL, &size, &hogPid);

	return hogPid;
}

bool
CoreAudioDevice::SetCurrentVolume(Float32 vol)
{
	if (!device_id || !has_volume)
		return false;

	AudioObjectPropertyAddress  aopa;
	aopa.mScope    = kAudioDevicePropertyScopeOutput;
	aopa.mElement  = 0;
	aopa.mSelector = kAudioHardwareServiceDeviceProperty_VirtualMasterVolume;

	OSStatus err = AudioObjectSetPropertyData(device_id, &aopa, 0, NULL, sizeof(Float32), &vol);
	if (err != noErr)
	{
		FormatError(macos_output_domain, "Unable to set output device volume. Error = %s", GetError(err).c_str());
		return false;
	}
	return true;
}

Float32
CoreAudioDevice::GetCurrentVolume()
{
	Float32 vol;
	UInt32 size = sizeof(vol);
	if (!device_id || !has_volume)
		return -1.0;
	
	AudioObjectPropertyAddress  aopa;
	aopa.mScope    = kAudioDevicePropertyScopeOutput;
	aopa.mElement  = 0;
	aopa.mSelector = kAudioHardwareServiceDeviceProperty_VirtualMasterVolume;
	
	OSStatus err = AudioObjectGetPropertyData(device_id, &aopa, 0, NULL, &size, &vol);
	if (err != noErr)
	{
		FormatError(macos_output_domain, "Unable to get output device volume. Error = %s", GetError(err).c_str());
		return -1.0;
	}
	return vol;
}

UInt32
CoreAudioDevice::GetBufferSize()
{
	if (!device_id)
		return 0;

	/** Return maximum of variable buffer property
	 *	which indicates the maximum buffer in case of
	 *	variable buffers, or standard buffer frame size
	 *	which is the minimum or regular buffer size.
	 */
	AudioObjectPropertyAddress  aopa;
	aopa.mScope    = kAudioDevicePropertyScopeOutput;
	aopa.mElement  = 0;
	aopa.mSelector = kAudioDevicePropertyUsesVariableBufferFrameSizes;

	UInt32 buffer_size = 0, var_buffer_size = 0, property_size;
	
	property_size = sizeof(var_buffer_size);
	OSStatus err = AudioObjectGetPropertyData(device_id, &aopa, 0, NULL, &property_size, &var_buffer_size);
	if (err != noErr)
	{
		// Ignore this error, variable buffer sizes rarely used
		var_buffer_size = 0;
	}
	aopa.mSelector = kAudioDevicePropertyBufferFrameSize;
	property_size = sizeof(buffer_size);
	err = AudioObjectGetPropertyData(device_id, &aopa, 0, NULL, &property_size, &buffer_size);
	if (err != noErr)
	{
		FormatError(macos_output_domain, "Unable to retrieve buffer frame size. Error = %s", GetError(err).c_str());
		buffer_size = 0;
	}
	return std::max(buffer_size, var_buffer_size);
}

bool
CoreAudioDevice::SetBufferSize(UInt32 size)
{
	if (!device_id)
		return false;
	
	AudioObjectPropertyAddress  aopa;
	aopa.mScope    = kAudioDevicePropertyScopeOutput;
	aopa.mElement  = 0;
	aopa.mSelector = kAudioDevicePropertyBufferFrameSize;

	OSStatus err;
	UInt32 property_size = sizeof(size);
	
	// Get buffer size for restore
	if(buffer_size_restore == 0)
	{
		UInt32 buffer_size;
		err = AudioObjectGetPropertyData(device_id, &aopa, 0, NULL, &property_size, &buffer_size);
		if(err != noErr)
			FormatError(macos_output_domain, "Unable to get initial buffer size. Error = %s", GetError(err).c_str());
		else
			buffer_size_restore = buffer_size;
	}
	err = AudioObjectSetPropertyData(device_id, &aopa, 0, NULL, property_size, &size);
	if (err != noErr)
	{
		FormatError(macos_output_domain, "Unable to set buffer size. Error = %s", GetError(err).c_str());
		return false;
	}
	return true;
}

float
CoreAudioDevice::ScoreSampleRate(Float64 destination_rate, unsigned int source_rate) const
{
	float score = 0;
	double int_portion;
	double frac_portion = modf(source_rate / destination_rate, &int_portion);
	// prefer sample rates that are multiples of the source sample rate
	score += (1 - frac_portion) * 1000;
	// prefer exact matches over other multiples
	score += (int_portion == 1.0) ? 500 : 0;
	// prefer higher multiples if source rate higher than dest rate
	if(source_rate >= destination_rate)
		score += (int_portion > 1 && int_portion < 100) ? (100 - int_portion) / 100 * 100 : 0;
	else
		score += (int_portion > 1 && int_portion < 100) ? (100 + int_portion) / 100 * 100 : 0;
	
	return score;
}

float
CoreAudioDevice::ScoreFormat(const AudioStreamBasicDescription &format_desc, const AudioFormat &format) const
{
	float score = 0;
	// Score only linear PCM formats (everything else MPD cannot use)
	if (format_desc.mFormatID == kAudioFormatLinearPCM)
	{
		score += ScoreSampleRate(format_desc.mSampleRate, format.sample_rate);
		
		// Just choose the stream / format with the highest number of output channels
		score += format_desc.mChannelsPerFrame * 5;
		
		if (format.format == SampleFormat::FLOAT)
		{
			// for float, prefer the highest bitdepth we have
			if (format_desc.mBitsPerChannel >= 16)
				score += (format_desc.mBitsPerChannel / 8);
		}
		else
		{
			if (format_desc.mBitsPerChannel == ((format.format == SampleFormat::S24_P32) ? 24 : format.GetSampleSize() * 8))
				score += 5;
			else if (format_desc.mBitsPerChannel > format.GetSampleSize() * 8)
				score += 1;
		}
	}
	return score;
}

AudioStreamBasicDescription CoreAudioDevice::GetPhysFormat()
{
	/** Always report back the (selected) physical
	 *	stream output format that will be set when
	 *	setting up the IOProc.
	 */
	return output_format;
}

AudioStreamBasicDescription CoreAudioDevice::GetIOFormat()
{
	/** Return the physical format of the device in case integer
	 *	mode is active (a non-mixable format has been selected).
	 *	Otherwise, the virtual format is 32bit native endian float
	 *	samples with the same sample rate and channels as the
	 *	physical format.
	 */
	AudioStreamBasicDescription io_format;
	if(output_format.mFormatFlags & kAudioFormatFlagIsNonMixable)
		return output_format;
	memset(&io_format, 0, sizeof(io_format));
	io_format.mFormatID = kAudioFormatLinearPCM;
	io_format.mChannelsPerFrame = output_format.mChannelsPerFrame;
	io_format.mSampleRate = output_format.mSampleRate;
	io_format.mFramesPerPacket = 1;
	io_format.mFormatFlags = kAudioFormatFlagIsFloat;
	if(IsBigEndian())
		io_format.mFormatFlags |= kAudioFormatFlagIsBigEndian;
	io_format.mBitsPerChannel = 32;
	io_format.mBytesPerFrame = io_format.mBytesPerPacket = sizeof(Float32) * io_format.mChannelsPerFrame;
	return io_format;
}

bool CoreAudioDevice::SetFormat(const AudioFormat &audio_format, bool prefer_unmixable)
{
	FormatDebug(macos_output_domain, "Finding CoreAudio stream for format %s.", ToString(audio_format).c_str());
	
	bool format_found  = false;
	memset(&output_format, 0, sizeof(output_format));
	float output_score  = 0;
	
	// loop over all streams to find one with a suitable format
	for(UInt32 stream_idx = 0; stream_idx < stream_infos.size(); stream_idx++)
	{
		
		// Probe physical formats
		const StreamFormatList *formats = &stream_infos[stream_idx].format_list;
		
		for (StreamFormatList::const_iterator j = formats->begin(); j != formats->end(); ++j)
		{
			AudioStreamBasicDescription format_desc = j->mFormat;
			std::string format_string;
			
			// for devices with kAudioStreamAnyRate
			// we use the requested samplerate here
			if (format_desc.mSampleRate == kAudioStreamAnyRate)
				format_desc.mSampleRate = audio_format.sample_rate;
			float score = 0;
			score = ScoreFormat(format_desc, audio_format);
			
			// For integer mode (unmixable format preferred) we change score based on the flag
			if (prefer_unmixable)
				format_desc.mFormatFlags & kAudioFormatFlagIsNonMixable ? score += 1.0 : score -= 1.0;
			
			FormatDebug(macos_output_domain, "Physical Format: %s rated %f", StreamDescriptionToString(format_desc, format_string), score);
			
			if (score > output_score)
			{
				output_score  = score;
				output_format = format_desc;
				output_stream_idx = stream_idx; // set the idx of the stream in the device
				format_found = true;
			}
		}
	}
	if (is_planar)
	{
		/** For planar devices make sure that the correct
		 *	format settings are forced here (this should
		 *	already be part of the format per default
		 *	and should therefore not be needed).
		 */
		output_format.mChannelsPerFrame = stream_infos.size();
		output_format.mFormatFlags |= kAudioFormatFlagIsNonInterleaved;
	}
	return format_found;
}
