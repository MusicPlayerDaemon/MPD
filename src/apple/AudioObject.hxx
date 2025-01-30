// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef APPLE_AUDIO_OBJECT_HXX
#define APPLE_AUDIO_OBJECT_HXX

#include "Throw.hxx"
#include "util/AllocatedArray.hxx"

#include <CoreAudio/AudioHardware.h>

#include <cstddef>

namespace Apple {
class StringRef;
}

inline std::size_t
AudioObjectGetPropertyDataSize(AudioObjectID inObjectID,
			       const AudioObjectPropertyAddress &inAddress)
{
	UInt32 size;
	OSStatus status = AudioObjectGetPropertyDataSize(inObjectID,
							 &inAddress,
							 0, nullptr, &size);
	if (status != noErr)
		Apple::ThrowOSStatus(status);

	return size;
}

template<typename T>
T
AudioObjectGetPropertyDataT(AudioObjectID inObjectID,
			    const AudioObjectPropertyAddress &inAddress)
{
	OSStatus status;
	UInt32 size = sizeof(T);
	T value;

	status = AudioObjectGetPropertyData(inObjectID, &inAddress,
					    0, nullptr,
					    &size, &value);
	if (status != noErr)
		Apple::ThrowOSStatus(status);

	return value;
}

Apple::StringRef
AudioObjectGetStringProperty(AudioObjectID inObjectID,
			     const AudioObjectPropertyAddress &inAddress);

template<typename T>
AllocatedArray<T>
AudioObjectGetPropertyDataArray(AudioObjectID inObjectID,
			       const AudioObjectPropertyAddress &inAddress)
{
	OSStatus status;
	UInt32 size;

	status = AudioObjectGetPropertyDataSize(inObjectID,
						&inAddress,
						0, nullptr, &size);
	if (status != noErr)
		Apple::ThrowOSStatus(status);

	AllocatedArray<T> result(size / sizeof(T));

	status = AudioObjectGetPropertyData(inObjectID, &inAddress,
					    0, nullptr,
					    &size, result.data());
	if (status != noErr)
		Apple::ThrowOSStatus(status);

	return result;
}

#endif
