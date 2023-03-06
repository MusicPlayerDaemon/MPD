// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AndroidMixerPlugin.hxx"
#include "mixer/Mixer.hxx"
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
