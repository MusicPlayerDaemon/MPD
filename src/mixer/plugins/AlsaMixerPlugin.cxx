// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AlsaMixerPlugin.hxx"
#include "VolumeMapping.hxx"
#include "lib/alsa/NonBlock.hxx"
#include "lib/alsa/Error.hxx"
#include "lib/alsa/PcmCard.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "mixer/Mixer.hxx"
#include "mixer/Listener.hxx"
#include "output/OutputAPI.hxx"
#include "event/MultiSocketMonitor.hxx"
#include "event/InjectEvent.hxx"
#include "event/Call.hxx"
#include "thread/Mutex.hxx"
#include "util/ASCII.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <alsa/asoundlib.h>
#include <cmath>

#include <fmt/format.h>

#include <iterator> // for std::back_inserter()
#include <string>

#define VOLUME_MIXER_ALSA_DEFAULT		"default"
#define VOLUME_MIXER_ALSA_CONTROL_DEFAULT	"PCM"
static constexpr unsigned VOLUME_MIXER_ALSA_INDEX_DEFAULT = 0;

class AlsaMixerMonitor final : MultiSocketMonitor {
	InjectEvent defer_invalidate_sockets;

	/**
	 * Protects the #snd_mixer_t (see AlsaMixer::handle_mutex).
	 */
	Mutex &mutex;

	snd_mixer_t *mixer;

	Alsa::NonBlockMixer non_block;

public:
	AlsaMixerMonitor(EventLoop &_loop, Mutex &_mutex,
			 snd_mixer_t *_mixer) noexcept
		:MultiSocketMonitor(_loop),
		 defer_invalidate_sockets(_loop,
					  BIND_THIS_METHOD(InvalidateSockets)),
		 mutex(_mutex),
		 mixer(_mixer) {
		defer_invalidate_sockets.Schedule();
	}

	~AlsaMixerMonitor() noexcept {
		BlockingCall(MultiSocketMonitor::GetEventLoop(), [this](){
				MultiSocketMonitor::Reset();
				defer_invalidate_sockets.Cancel();
			});
	}

	AlsaMixerMonitor(const AlsaMixerMonitor &) = delete;
	AlsaMixerMonitor &operator=(const AlsaMixerMonitor &) = delete;

private:
	Event::Duration PrepareSockets() noexcept override;
	void DispatchSockets() noexcept override;
};

class AlsaMixer final : public Mixer {
	EventLoop &event_loop;

	/**
	 * The "mixer_device" setting (nullptr if not configured).
	 */
	const char *mixer_device;

	/**
	 * The "device" setting of the ALSA output (nullptr if not
	 * configured).  Its sound card is used if "mixer_device" is
	 * not configured.
	 */
	const char *pcm_device;

	/**
	 * The mixer device which is actually used; determined by
	 * Open().
	 */
	std::string device;

	const char *control;
	unsigned int index;

	/**
	 * Protects #handle, #elem, #desired_volume and
	 * #resulting_volume while #monitor exists, because libasound
	 * is not thread-safe, and #monitor uses the #snd_mixer_t in
	 * the #EventLoop thread while SetVolume() and GetVolume() are
	 * called from other threads.
	 */
	Mutex handle_mutex;

	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;

	AlsaMixerMonitor *monitor;

	/**
	 * These fields are our workaround for rounding errors when
	 * the resolution of a mixer knob isn't fine enough to
	 * represent all 101 possible values (0..100).
	 *
	 * "desired_volume" is the percent value passed to
	 * SetVolume(), and "resulting_volume" is the volume which was
	 * actually set, and would be returned by the next
	 * GetPercentVolume() call.
	 *
	 * When GetVolume() is called, we compare the
	 * "resulting_volume" with the value returned by
	 * GetPercentVolume(), and if it's the same, we're still on
	 * the same value that was previously set (but may have been
	 * rounded down or up).
	 */
	int desired_volume, resulting_volume;

public:
	AlsaMixer(EventLoop &_event_loop, MixerListener &_listener) noexcept
		:Mixer(alsa_mixer_plugin, _listener),
		 event_loop(_event_loop) {}

	~AlsaMixer() noexcept override;

	AlsaMixer(const AlsaMixer &) = delete;
	AlsaMixer &operator=(const AlsaMixer &) = delete;

	void Configure(const ConfigBlock &block);
	std::string DetermineDevice() const;
	void Setup();

	/* virtual methods from class Mixer */
	void Open() override;
	void Close() noexcept override;
	int GetVolume() override;
	void SetVolume(unsigned volume) override;
	unsigned ScaleVolume(unsigned volume, float scale) noexcept override;

private:
	[[gnu::const]]
	static unsigned NormalizedToPercent(double normalized) noexcept {
		return std::lround(100 * normalized);
	}

	[[gnu::pure]]
	[[nodiscard]] double GetNormalizedVolume() const noexcept {
		return get_normalized_playback_volume(elem,
						      SND_MIXER_SCHN_FRONT_LEFT);
	}

	[[gnu::pure]]
	[[nodiscard]] unsigned GetPercentVolume() const noexcept {
		return NormalizedToPercent(GetNormalizedVolume());
	}

	/**
	 * Called by snd_mixer_handle_events() while #handle_mutex is
	 * locked.
	 */
	static int ElemCallback(snd_mixer_elem_t *elem,
				unsigned mask) noexcept;

};

static constexpr Domain alsa_mixer_domain("alsa_mixer");

Event::Duration
AlsaMixerMonitor::PrepareSockets() noexcept
{
	if (mixer == nullptr) {
		ClearSocketList();
		return Event::Duration(-1);
	}

	const std::scoped_lock lock{mutex};
	return non_block.PrepareSockets(*this, mixer);
}

void
AlsaMixerMonitor::DispatchSockets() noexcept
{
	assert(mixer != nullptr);

	const std::scoped_lock lock{mutex};

	non_block.DispatchSockets(*this, mixer);

	int err = snd_mixer_handle_events(mixer);
	if (err < 0) {
		FmtError(alsa_mixer_domain,
			 "snd_mixer_handle_events() failed: {}",
			 snd_strerror(err));

		if (err == -ENODEV) {
			/* the sound device was unplugged; disable
			   this GSource */
			mixer = nullptr;
			InvalidateSockets();
			return;
		}
	}
}

/*
 * libasound callbacks
 *
 */

int
AlsaMixer::ElemCallback(snd_mixer_elem_t *elem, unsigned mask) noexcept
{
	AlsaMixer &mixer = *(AlsaMixer *)
		snd_mixer_elem_get_callback_private(elem);

	if (mask & SND_CTL_EVENT_MASK_VALUE) {
		int volume = mixer.GetPercentVolume();

		if (mixer.resulting_volume >= 0 &&
		    volume == mixer.resulting_volume)
			/* still the same volume (this might be a
			   callback caused by SetVolume()) - switch to
			   desired_volume */
			volume = mixer.desired_volume;
		else
			/* flush */
			mixer.desired_volume = mixer.resulting_volume = -1;

		mixer.listener.OnMixerVolumeChanged(mixer, volume);
	}

	return 0;
}

/*
 * mixer_plugin methods
 *
 */

inline void
AlsaMixer::Configure(const ConfigBlock &block)
{
	mixer_device = block.GetBlockValue("mixer_device");
	pcm_device = block.GetBlockValue("device");
	control = block.GetBlockValue("mixer_control",
				      VOLUME_MIXER_ALSA_CONTROL_DEFAULT);
	index = block.GetBlockValue("mixer_index",
				    VOLUME_MIXER_ALSA_INDEX_DEFAULT);
}

static Mixer *
alsa_mixer_init(EventLoop &event_loop, [[maybe_unused]] AudioOutput &ao,
		MixerListener &listener,
		const ConfigBlock &block)
{
	auto *am = new AlsaMixer(event_loop, listener);
	am->Configure(block);

	return am;
}

AlsaMixer::~AlsaMixer() noexcept
{
	/* free libasound's config cache */
	snd_config_update_free_global();
}

[[gnu::pure]]
static snd_mixer_elem_t *
alsa_mixer_lookup_elem(snd_mixer_t *handle,
		       const char *name, unsigned idx) noexcept
{
	for (snd_mixer_elem_t *elem = snd_mixer_first_elem(handle);
	     elem != nullptr; elem = snd_mixer_elem_next(elem)) {
		if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE &&
		    StringEqualsCaseASCII(snd_mixer_selem_get_name(elem),
					  name) &&
		    snd_mixer_selem_get_index(elem) == idx)
			return elem;
	}

	return nullptr;
}

/**
 * Build a human-readable list of all simple mixer controls (for
 * error messages).
 */
static std::string
alsa_mixer_list_controls(snd_mixer_t *handle)
{
	std::string result;

	for (snd_mixer_elem_t *elem = snd_mixer_first_elem(handle);
	     elem != nullptr; elem = snd_mixer_elem_next(elem)) {
		if (snd_mixer_elem_get_type(elem) != SND_MIXER_ELEM_SIMPLE)
			continue;

		if (!result.empty())
			result += ", ";

		fmt::format_to(std::back_inserter(result), "{:?},{}",
			       snd_mixer_selem_get_name(elem),
			       snd_mixer_selem_get_index(elem));
	}

	if (result.empty())
		result = "none";

	return result;
}

inline std::string
AlsaMixer::DetermineDevice() const
{
	if (mixer_device != nullptr)
		return mixer_device;

	if (pcm_device != nullptr) {
		/* use the sound card of the PCM device */
		if (auto card = Alsa::FindPcmCard(pcm_device);
		    !card.empty()) {
			FmtDebug(alsa_mixer_domain,
				 "Using mixer device {:?} for PCM device {:?}",
				 card, pcm_device);
			return card;
		}
	}

	return VOLUME_MIXER_ALSA_DEFAULT;
}

inline void
AlsaMixer::Setup()
{
	int err;

	if ((err = snd_mixer_attach(handle, device.c_str())) < 0)
		throw Alsa::MakeError(err,
				      FmtBuffer<256>("failed to attach to {}",
						     device));

	if ((err = snd_mixer_selem_register(handle, nullptr, nullptr)) < 0)
		throw Alsa::MakeError(err, "snd_mixer_selem_register() failed");

	if ((err = snd_mixer_load(handle)) < 0)
		throw Alsa::MakeError(err, "snd_mixer_load() failed");

	elem = alsa_mixer_lookup_elem(handle, control, index);
	if (elem == nullptr)
		throw FmtRuntimeError("no such mixer control {:?},{} on {:?} (available: {})",
				      control, index, device,
				      alsa_mixer_list_controls(handle));

	snd_mixer_elem_set_callback_private(elem, this);
	snd_mixer_elem_set_callback(elem, ElemCallback);

	monitor = new AlsaMixerMonitor(event_loop, handle_mutex, handle);
}

void
AlsaMixer::Open()
{
	desired_volume = resulting_volume = -1;

	device = DetermineDevice();

	int err;

	err = snd_mixer_open(&handle, 0);
	if (err < 0)
		throw Alsa::MakeError(err, "snd_mixer_open() failed");

	try {
		Setup();
	} catch (...) {
		snd_mixer_close(handle);
		throw;
	}
}

void
AlsaMixer::Close() noexcept
{
	assert(handle != nullptr);

	/* after this, the #EventLoop thread doesn't use #handle
	   anymore, therefore we don't need to lock #handle_mutex */
	delete monitor;

	snd_mixer_elem_set_callback(elem, nullptr);
	snd_mixer_close(handle);
}

int
AlsaMixer::GetVolume()
{
	int err;

	assert(handle != nullptr);

	const std::scoped_lock lock{handle_mutex};

	err = snd_mixer_handle_events(handle);
	if (err < 0)
		throw Alsa::MakeError(err, "snd_mixer_handle_events() failed");

	int volume = GetPercentVolume();
	if (resulting_volume >= 0 && volume == resulting_volume)
		/* we're still on the value passed to SetVolume() */
		volume = desired_volume;

	return volume;
}

void
AlsaMixer::SetVolume(unsigned volume)
{
	assert(handle != nullptr);

	const std::scoped_lock lock{handle_mutex};

	int err = set_normalized_playback_volume(elem, 0.01*volume, 1);
	if (err < 0)
		throw Alsa::MakeError(err, "failed to set ALSA volume");

	desired_volume = volume;
	resulting_volume = GetPercentVolume();
}

unsigned
AlsaMixer::ScaleVolume(unsigned volume, float scale) noexcept
{
	assert(handle != nullptr);

	return NormalizedToPercent(scale_normalized_playback_volume(elem,
								    0.01 * volume,
								    (double)scale));
}

const MixerPlugin alsa_mixer_plugin = {
	alsa_mixer_init,
	true,
};
