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

#include "CoreAudioStream.hxx"
#include "CoreAudioHelpers.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

static constexpr Domain macos_output_domain("macos_output");

CoreAudioStream::CoreAudioStream() :
	stream_id  (0    )
{
	original_virtual_fmt.mFormatID = 0;
	original_physical_fmt.mFormatID = 0;
}

CoreAudioStream::~CoreAudioStream()
{
	Close();
}

bool CoreAudioStream::Open(AudioStreamID id)
{
	stream_id = id;
	FormatDebug(macos_output_domain, "Opened stream 0x%04x.", (uint)stream_id);
	
	// Get original stream formats
	if (!GetVirtualFormat(&original_virtual_fmt))
	{
		FormatError(macos_output_domain, "Unable to retrieve current virtual format for stream 0x%04x.", (uint)stream_id);
		return false;
	}
	if (!GetPhysicalFormat(&original_physical_fmt))
	{
		FormatError(macos_output_domain, "Unable to retrieve current physical format for stream 0x%04x.",
					(uint)stream_id);
		return false;
	}

	// watch for physical property changes.
	AudioObjectPropertyAddress aopa;
	aopa.mScope    = kAudioObjectPropertyScopeGlobal;
	aopa.mElement  = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyPhysicalFormat;
	if (AudioObjectAddPropertyListener(stream_id, &aopa, HardwareStreamListener, this) != noErr)
		FormatError(macos_output_domain,"Couldn't set up a physical property listener for Core Audio stream.");

	// watch for virtual property changes.
	aopa.mScope    = kAudioObjectPropertyScopeGlobal;
	aopa.mElement  = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyVirtualFormat;
	if (AudioObjectAddPropertyListener(stream_id, &aopa, HardwareStreamListener, this) != noErr)
		FormatError(macos_output_domain, "Couldn't set up a virtual property listener for Core Audio stream.");

	return true;
}


void CoreAudioStream::Close(bool restore)
{
	std::string format_string;
	if (!stream_id)
		return;

	// remove the physical/virtual property listeners
	AudioObjectPropertyAddress aopa;
	aopa.mScope    = kAudioObjectPropertyScopeGlobal;
	aopa.mElement  = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyPhysicalFormat;
	if (AudioObjectRemovePropertyListener(stream_id, &aopa, HardwareStreamListener, this) != noErr)
		FormatDebug(macos_output_domain, "Couldn't remove property listener for Core Audio stream.");

	aopa.mScope    = kAudioObjectPropertyScopeGlobal;
	aopa.mElement  = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyVirtualFormat;
	if (AudioObjectRemovePropertyListener(stream_id, &aopa, HardwareStreamListener, this) != noErr)
		FormatDebug(macos_output_domain, "Couldn't remove property listener for Core Audio stream.");

	// Revert any format changes we made
	if (restore && original_virtual_fmt.mFormatID && stream_id)
	{
		FormatDebug(macos_output_domain, "Restoring original virtual format for stream 0x%04x. (%s)",
					(uint)stream_id, StreamDescriptionToString(original_virtual_fmt, format_string));
		AudioStreamBasicDescription setFormat = original_virtual_fmt;
		SetVirtualFormat(&setFormat);
	}
	if (restore && original_physical_fmt.mFormatID && stream_id)
	{
		FormatDebug(macos_output_domain, "Restoring original physical format for stream 0x%04x. (%s)",
					(uint)stream_id, StreamDescriptionToString(original_physical_fmt, format_string));
		AudioStreamBasicDescription setFormat = original_physical_fmt;
		SetPhysicalFormat(&setFormat);
	}

	original_virtual_fmt.mFormatID  = 0;
	original_physical_fmt.mFormatID = 0;
	FormatDebug(macos_output_domain, "Closed stream 0x%04x.", (uint)stream_id);
	stream_id = 0;
}

bool CoreAudioStream::GetStartingChannelInDevice(AudioStreamID id, UInt32 &startingChannel)
{
	if (!id)
		return 0;

	UInt32 i_param_size = sizeof(UInt32);
	UInt32 i_param;
	startingChannel = 0;
	bool ret = false;

	AudioObjectPropertyAddress aopa;
	aopa.mScope    = kAudioObjectPropertyScopeGlobal;
	aopa.mElement  = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyStartingChannel;

	OSStatus status = AudioObjectGetPropertyData(id, &aopa, 0, NULL, &i_param_size, &i_param);
	if (status == noErr)
	{
		startingChannel = i_param;
		ret = true;
	}
	return ret;
}

bool CoreAudioStream::GetVirtualFormat(AudioStreamBasicDescription* desc)
{
	if (!desc || !stream_id)
		return false;

	UInt32 size = sizeof(AudioStreamBasicDescription);

	AudioObjectPropertyAddress aopa;
	aopa.mScope    = kAudioObjectPropertyScopeGlobal;
	aopa.mElement  = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyVirtualFormat;
	OSStatus ret = AudioObjectGetPropertyDataSize(stream_id, &aopa, 0, NULL, &size);
	if (ret)
		return false;

	ret = AudioObjectGetPropertyData(stream_id, &aopa, 0, NULL, &size, desc);
	if (ret)
		return false;
	return true;
}

bool CoreAudioStream::SetVirtualFormat(AudioStreamBasicDescription* desc)
{
	if (!desc || !stream_id)
		return false;

	std::string format_string;

	AudioObjectPropertyAddress aopa;
	aopa.mScope    = kAudioObjectPropertyScopeGlobal;
	aopa.mElement  = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyVirtualFormat;

	UInt32 property_size = sizeof(AudioStreamBasicDescription);
	OSStatus ret = AudioObjectSetPropertyData(stream_id, &aopa, 0, NULL, property_size, desc);
	if (ret)
	{
		FormatError(macos_output_domain, "Unable to set virtual format for stream 0x%04x. Error = %s",
					(uint)stream_id, GetError(ret).c_str());
		return false;
	}
	return true;
}

bool CoreAudioStream::GetPhysicalFormat(AudioStreamBasicDescription* desc)
{
	if (!desc || !stream_id)
		return false;

	UInt32 size = sizeof(AudioStreamBasicDescription);

	AudioObjectPropertyAddress aopa;
	aopa.mScope    = kAudioObjectPropertyScopeGlobal;
	aopa.mElement  = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyPhysicalFormat;

	OSStatus ret = AudioObjectGetPropertyData(stream_id, &aopa, 0, NULL, &size, desc);
	if (ret)
		return false;
	return true;
}

bool CoreAudioStream::SetPhysicalFormat(AudioStreamBasicDescription* desc)
{
	if (!desc || !stream_id)
		return false;

	std::string format_string;
	
	AudioObjectPropertyAddress aopa;
	aopa.mScope    = kAudioObjectPropertyScopeGlobal;
	aopa.mElement  = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyPhysicalFormat;

	UInt32 property_size = sizeof(AudioStreamBasicDescription);
	OSStatus ret = AudioObjectSetPropertyData(stream_id, &aopa, 0, NULL, property_size, desc);
	if (ret)
	{
		FormatError(macos_output_domain, "Unable to set physical format for stream 0x%04x. Error = %s",
					(uint)stream_id, GetError(ret).c_str());
		return false;
	}
	return true;
}

bool CoreAudioStream::GetAvailablePhysicalFormats(StreamFormatList* stream_fmt_list)
{
	return GetAvailablePhysicalFormats(stream_id, stream_fmt_list);
}

bool CoreAudioStream::GetAvailablePhysicalFormats(AudioStreamID id, StreamFormatList* stream_fmt_list)
{
	if (!stream_fmt_list || !id)
		return false;

	AudioObjectPropertyAddress aopa;
	aopa.mScope    = kAudioObjectPropertyScopeGlobal;
	aopa.mElement  = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyAvailablePhysicalFormats;

	UInt32 property_size = 0;
	OSStatus err = AudioObjectGetPropertyDataSize(id, &aopa, 0, NULL, &property_size);
	if (err != noErr)
		return false;

	UInt32 format_count = property_size / sizeof(AudioStreamRangedDescription);
	AudioStreamRangedDescription *format_list = new AudioStreamRangedDescription[format_count];
	err = AudioObjectGetPropertyData(id, &aopa, 0, NULL, &property_size, format_list);
	if (err == noErr)
	{
		for (UInt32 format = 0; format < format_count; format++)
			stream_fmt_list->push_back(format_list[format]);
	}
	delete[] format_list;
	return (err == noErr);
}

OSStatus CoreAudioStream::HardwareStreamListener(gcc_unused AudioObjectID inObjectID,
												  UInt32 inNumberAddresses,
												  const AudioObjectPropertyAddress inAddresses[],
												  void *inClientData)
{
	CoreAudioStream *ca_stream = (CoreAudioStream*)inClientData;
	std::string format_string;

	for (UInt32 i = 0; i < inNumberAddresses; i++)
	{
		if (inAddresses[i].mSelector == kAudioStreamPropertyPhysicalFormat)
		{
			AudioStreamBasicDescription actual_format;
			UInt32 property_size = sizeof(AudioStreamBasicDescription);
			// hardware physical format has changed.
			if (AudioObjectGetPropertyData(ca_stream->stream_id, &inAddresses[i], 0, NULL, &property_size, &actual_format) == noErr)
			{
				FormatDebug(macos_output_domain, "Hardware physical format changed to %s",
						   StreamDescriptionToString(actual_format, format_string));
			}
		}
		else if (inAddresses[i].mSelector == kAudioStreamPropertyVirtualFormat)
		{
			// hardware virtual format has changed.
			AudioStreamBasicDescription actual_format;
			UInt32 property_size = sizeof(AudioStreamBasicDescription);
			if (AudioObjectGetPropertyData(ca_stream->stream_id, &inAddresses[i], 0, NULL, &property_size, &actual_format) == noErr)
			{
				FormatDebug(macos_output_domain, "Hardware virtual format changed to %s",
						   StreamDescriptionToString(actual_format, format_string));
			}
		}
	}
  return noErr;
}
