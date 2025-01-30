// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "AudioObject.hxx"
#include "StringRef.hxx"

Apple::StringRef
AudioObjectGetStringProperty(AudioObjectID inObjectID,
			     const AudioObjectPropertyAddress &inAddress)
{
	auto s = AudioObjectGetPropertyDataT<CFStringRef>(inObjectID,
							  inAddress);
	return Apple::StringRef(s);
}
