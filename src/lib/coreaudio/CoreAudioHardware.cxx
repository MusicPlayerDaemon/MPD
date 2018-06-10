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

#include "CoreAudioHardware.hxx"
#include "CoreAudioHelpers.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

static constexpr Domain macos_output_domain("macos_output");

AudioDeviceID CCoreAudioHardware::FindAudioDevice(const std::string &search_name)
{
	AudioDeviceID device_id = 0;

	if (!search_name.length())
		return device_id;

	std::string search_name_lower = search_name;
	std::transform(search_name_lower.begin(), search_name_lower.end(), search_name_lower.begin(), ::tolower );
	if (search_name_lower.compare("default") == 0)
	{
		AudioDeviceID default_device = GetDefaultOutputDevice();
		FormatDebug(macos_output_domain, "Returning default device [0x%04x].", (uint)default_device);
		return default_device;
	}
	FormatDebug(macos_output_domain, "Searching for device - %s.", search_name.c_str());
	// Obtain a list of all available audio devices
	AudioObjectPropertyAddress aopa;
	aopa.mScope    = kAudioObjectPropertyScopeGlobal;
	aopa.mElement  = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioHardwarePropertyDevices;

	UInt32 size = 0;
	OSStatus ret = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &aopa, 0, NULL, &size);
	if (ret != noErr)
	{
		FormatError(macos_output_domain, "Unable to retrieve the size of the list of available devices. Error = %s",
					GetError(ret).c_str());
		return 0;
	}

	size_t device_count = size / sizeof(AudioDeviceID);
	AudioDeviceID* device_list = new AudioDeviceID[device_count];
	ret = AudioObjectGetPropertyData(kAudioObjectSystemObject, &aopa, 0, NULL, &size, device_list);
	if (ret != noErr)
	{
		FormatError(macos_output_domain, "Unable to retrieve the list of available devices. Error = %s",
					GetError(ret).c_str());
		delete[] device_list;
		return 0;
	}

	// Attempt to locate the requested device
	std::string device_name;
	for (size_t dev = 0; dev < device_count; dev++)
	{
		CoreAudioDevice device(device_list[dev]);
		device_name = device.GetName();
		std::transform(device_name.begin(), device_name.end(), device_name.begin(), ::tolower );
		if (search_name_lower.compare(device_name) == 0)
			device_id = device_list[dev];
		if (device_id)
			break;
	}
	delete[] device_list;

	return device_id;
}

AudioDeviceID CCoreAudioHardware::GetDefaultOutputDevice()
{
	AudioDeviceID device_id = 0;

	AudioObjectPropertyAddress  aopa;
	aopa.mScope    = kAudioObjectPropertyScopeGlobal;
	aopa.mElement  = kAudioObjectPropertyElementMaster;
	aopa.mSelector = kAudioHardwarePropertyDefaultOutputDevice;

	UInt32 size = sizeof(AudioDeviceID);
	OSStatus ret = AudioObjectGetPropertyData(kAudioObjectSystemObject, &aopa, 0, NULL, &size, &device_id);

	// outputDevice is set to 0 if there is no audio device available
	if (ret != noErr || !device_id)
	{
		FormatError(macos_output_domain, "Unable to identify default output device. Error = %s",
					GetError(ret).c_str());
		return 0;
	}
	
	return device_id;
}
