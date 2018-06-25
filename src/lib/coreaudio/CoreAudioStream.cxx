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
#include "util/RuntimeError.hxx"

static constexpr Domain macos_output_domain("macos_output");

CoreAudioStream::CoreAudioStream() {
	original_virtual_fmt.mFormatID = 0;
	original_physical_fmt.mFormatID = 0;
}

CoreAudioStream::~CoreAudioStream() {
	Close();
}

void
CoreAudioStream::Open(AudioStreamID id) {
	stream_id = id;
	FormatDebug(macos_output_domain, "Opening stream 0x%04x.", (uint)stream_id);
	
	// Get original stream formats
	original_virtual_fmt = GetVirtualFormat();
	original_physical_fmt = GetPhysicalFormat();

	// watch for physical property changes.
	AudioObjectPropertyAddress aopa;
	aopa.mScope = kAudioObjectPropertyScopeGlobal;
	aopa.mElement = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyPhysicalFormat;
	if (AudioObjectAddPropertyListener(stream_id, &aopa, HardwareStreamListener, this) != noErr)
		throw std::runtime_error("Couldn't set up a physical stream format property listener for Core Audio stream.");

	// watch for virtual property changes.
	aopa.mSelector = kAudioStreamPropertyVirtualFormat;
	if (AudioObjectAddPropertyListener(stream_id, &aopa, HardwareStreamListener, this) != noErr)
		throw std::runtime_error("Couldn't set up a virtual stream format property listener for Core Audio stream.");

}


void
CoreAudioStream::Close() noexcept {
	if (!stream_id)
		return;

	// remove the physical/virtual property listeners
	AudioObjectPropertyAddress aopa;
	aopa.mScope = kAudioObjectPropertyScopeGlobal;
	aopa.mElement = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyPhysicalFormat;
	try {
		if (AudioObjectRemovePropertyListener(stream_id, &aopa, HardwareStreamListener, this) != noErr)
			FormatWarning(macos_output_domain, "Couldn't remove property listener for Core Audio stream.");
		aopa.mSelector = kAudioStreamPropertyVirtualFormat;
		if (AudioObjectRemovePropertyListener(stream_id, &aopa, HardwareStreamListener, this) != noErr)
			FormatWarning(macos_output_domain, "Couldn't remove property listener for Core Audio stream.");

		// Revert any format changes we made
		if (original_virtual_fmt.mFormatID) {
			FormatDebug(macos_output_domain, "Restoring original virtual format for stream 0x%04x. (%s)", (uint)stream_id, StreamDescriptionToString(original_virtual_fmt).c_str());
			SetVirtualFormat(original_virtual_fmt);
		}
		if (original_physical_fmt.mFormatID) {
			FormatDebug(macos_output_domain, "Restoring original physical format for stream 0x%04x. (%s)", (uint)stream_id, StreamDescriptionToString(original_physical_fmt).c_str());
			SetPhysicalFormat(original_physical_fmt);
		}
	}
	catch (...) {
		// Ignore any exceptions that might be thrown, as the stream is anyways closed.
	}
	original_virtual_fmt.mFormatID  = 0;
	original_physical_fmt.mFormatID = 0;
	FormatDebug(macos_output_domain, "Closed stream 0x%04x.", (uint)stream_id);
	stream_id = 0;
}

AudioStreamBasicDescription
CoreAudioStream::GetVirtualFormat() {
	if (!stream_id)
		throw std::runtime_error("Invalid stream ID.");
	AudioStreamBasicDescription desc;
	UInt32 size = sizeof(desc);

	AudioObjectPropertyAddress aopa;
	aopa.mScope = kAudioObjectPropertyScopeGlobal;
	aopa.mElement = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyVirtualFormat;

	OSStatus err = AudioObjectGetPropertyData(stream_id, &aopa, 0, NULL, &size, &desc);
	if (err != noErr)
		throw FormatRuntimeError("Unable to retrieve virtual format for stream 0x%04x.", (uint)stream_id);
	return desc;
}

void
CoreAudioStream::SetVirtualFormat(AudioStreamBasicDescription desc) {
	if (!stream_id)
		return;

	AudioObjectPropertyAddress aopa;
	aopa.mScope = kAudioObjectPropertyScopeGlobal;
	aopa.mElement = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyVirtualFormat;

	UInt32 property_size = sizeof(AudioStreamBasicDescription);
	OSStatus err = AudioObjectSetPropertyData(stream_id, &aopa, 0, NULL, property_size, &desc);
	if (err != noErr)
		throw FormatRuntimeError("Unable to set virtual format for stream 0x%04x. Error = %s", (uint)stream_id, GetError(err));
}

AudioStreamBasicDescription
CoreAudioStream::GetPhysicalFormat() {
	if (!stream_id)
		throw std::runtime_error("Invalid stream ID.");

	AudioStreamBasicDescription desc;
	UInt32 size = sizeof(desc);

	AudioObjectPropertyAddress aopa;
	aopa.mScope = kAudioObjectPropertyScopeGlobal;
	aopa.mElement = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyPhysicalFormat;

	OSStatus err = AudioObjectGetPropertyData(stream_id, &aopa, 0, NULL, &size, &desc);
	if (err != noErr)
		throw FormatRuntimeError("Unable to retrieve physical format for stream 0x%04x.", (uint)stream_id);
	return desc;
}

void
CoreAudioStream::SetPhysicalFormat(AudioStreamBasicDescription desc) {
	if (!stream_id)
		return;

	AudioObjectPropertyAddress aopa;
	aopa.mScope = kAudioObjectPropertyScopeGlobal;
	aopa.mElement = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyPhysicalFormat;

	UInt32 property_size = sizeof(AudioStreamBasicDescription);
	OSStatus err = AudioObjectSetPropertyData(stream_id, &aopa, 0, NULL, property_size, &desc);
	if (err != noErr)
		throw FormatRuntimeError("Unable to set physical format for stream 0x%04x. Error = %s", (uint)stream_id, GetError(err));
}

StreamFormatList
CoreAudioStream::GetAvailablePhysicalFormats() {
	return GetAvailablePhysicalFormats(stream_id);
}

StreamFormatList
CoreAudioStream::GetAvailablePhysicalFormats(AudioStreamID id) {
	if (!id)
		throw std::runtime_error("Invalid stream ID.");

	StreamFormatList stream_fmt_list;
	
	AudioObjectPropertyAddress aopa;
	aopa.mScope = kAudioObjectPropertyScopeGlobal;
	aopa.mElement = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioStreamPropertyAvailablePhysicalFormats;

	UInt32 property_size = 0;
	OSStatus err = AudioObjectGetPropertyDataSize(id, &aopa, 0, NULL, &property_size);
	if (err != noErr)
		throw FormatRuntimeError("Unable to get available formats for stream 0x%04x. Error = %s", (uint)id, GetError(err));

	UInt32 format_count = property_size / sizeof(AudioStreamRangedDescription);
	AudioStreamRangedDescription *format_list = new AudioStreamRangedDescription[format_count];
	
	try {
		err = AudioObjectGetPropertyData(id, &aopa, 0, NULL, &property_size, format_list);
		if (err != noErr)
			throw FormatRuntimeError("Unable to get available formats for stream 0x%04x. Error = %s", (uint)id, GetError(err));
		for (unsigned int format = 0; format < format_count; format++)
			stream_fmt_list.push_back(format_list[format]);
	}
	catch (...) {
		delete[] format_list;
		throw;
	}
	delete[] format_list;
	return stream_fmt_list;
}

OSStatus CoreAudioStream::HardwareStreamListener(gcc_unused AudioObjectID inObjectID, UInt32 inNumberAddresses, const AudioObjectPropertyAddress inAddresses[], void *inClientData) {
	CoreAudioStream *ca_stream = (CoreAudioStream*)inClientData;
	for (unsigned int i = 0; i < inNumberAddresses; i++) {
		if (inAddresses[i].mSelector == kAudioStreamPropertyPhysicalFormat) {
			AudioStreamBasicDescription actual_format;
			UInt32 property_size = sizeof(AudioStreamBasicDescription);
			// hardware physical format has changed.
			if (AudioObjectGetPropertyData(ca_stream->stream_id, &inAddresses[i], 0, NULL, &property_size, &actual_format) == noErr)
				FormatDebug(macos_output_domain, "Hardware physical format changed to %s", StreamDescriptionToString(actual_format).c_str());
		}
		else if (inAddresses[i].mSelector == kAudioStreamPropertyVirtualFormat) {
			// hardware virtual format has changed.
			AudioStreamBasicDescription actual_format;
			UInt32 property_size = sizeof(AudioStreamBasicDescription);
			if (AudioObjectGetPropertyData(ca_stream->stream_id, &inAddresses[i], 0, NULL, &property_size, &actual_format) == noErr)
				FormatDebug(macos_output_domain, "Hardware virtual format changed to %s", StreamDescriptionToString(actual_format).c_str());
		}
	}
	return noErr;
}
