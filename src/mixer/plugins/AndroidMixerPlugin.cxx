/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "mixer/MixerInternal.hxx"
#include "filter/plugins/VolumeFilterPlugin.hxx"
#include "pcm/Volume.hxx"
#include "android/Context.hxx"
#include "android/AudioManager.hxx"

#include "Main.hxx"

#include <cassert>
#include <cmath>

class AndroidMixer final : public Mixer {
	AudioManager *audioManager;
	int currentVolume;
	int maxAndroidVolume;
	int lastAndroidVolume;
public:
	explicit AndroidMixer(MixerListener &_listener);

	~AndroidMixer() override;

	/* virtual methods from class Mixer */
	void Open() override {
	}

	void Close() noexcept override {
	}

	int GetVolume() override;

	void SetVolume(unsigned volume) override;
};

static Mixer *
android_mixer_init([[maybe_unused]] EventLoop &event_loop,
		    [[maybe_unused]] AudioOutput &ao,
		    MixerListener &listener,
		    [[maybe_unused]] const ConfigBlock &block)
{
	return new AndroidMixer(listener);
}

AndroidMixer::AndroidMixer(MixerListener &_listener)
	:Mixer(android_mixer_plugin, _listener)
{
	JNIEnv *env = Java::GetEnv();
	audioManager = context->GetAudioManager(env);

	maxAndroidVolume = audioManager->GetMaxVolume();
	if (maxAndroidVolume != 0)
	{
		lastAndroidVolume = audioManager->GetVolume(env);
		currentVolume = 100 * lastAndroidVolume / maxAndroidVolume;
	}
}

AndroidMixer::~AndroidMixer()
{
	delete audioManager;
}

int
AndroidMixer::GetVolume()
{
	JNIEnv *env = Java::GetEnv();
	if (maxAndroidVolume == 0)
		return -1;

	// The android volume index (or scale) is very likely inferior to the
	// MPD one (100). The last volume set by MPD is saved into
	// currentVolume, this volume is returned instead of the Android one
	// when the Android mixer was not touched by an other application. This
	// allows to fake a 0..100 scale from MPD.

	int volume = audioManager->GetVolume(env);
	if (volume == lastAndroidVolume)
		return currentVolume;

	return 100 * volume / maxAndroidVolume;
}

void
AndroidMixer::SetVolume(unsigned newVolume)
{
	JNIEnv *env = Java::GetEnv();
	if (maxAndroidVolume == 0)
		return;
	currentVolume = newVolume;
	lastAndroidVolume = currentVolume * maxAndroidVolume / 100;
	audioManager->SetVolume(env, lastAndroidVolume);

}

const MixerPlugin android_mixer_plugin = {
	android_mixer_init,
	true,
};
