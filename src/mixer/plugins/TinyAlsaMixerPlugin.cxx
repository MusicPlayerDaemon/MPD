// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "TinyAlsaMixerPlugin.hxx"
#include "DesiredVolume.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "mixer/Mixer.hxx"
#include "mixer/MixerPlugin.hxx"
#include "config/Block.hxx"
#include "system/Error.hxx"

#include <tinyalsa/mixer.h>

#include <fmt/format.h>

#include <algorithm> // for std::fill_n()
#include <array>
#include <cassert>
#include <cmath>
#include <iterator> // for std::back_inserter()
#include <string>

static constexpr char DEFAULT_CONTROL[] = "PCM Playback Volume";

/**
 * The maximum number of values of an integer mixer control (see
 * struct snd_ctl_elem_value).
 */
static constexpr unsigned MAX_VALUES = 128;

class TinyAlsaMixer final : public Mixer {
	/**
	 * The sound card number (the "card" setting of the output).
	 */
	const unsigned card;

	/**
	 * The name of the mixer control.
	 */
	const std::string control;

	/**
	 * The index of the mixer control, for cards which have more
	 * than one control with the same name.
	 */
	const unsigned index;

	struct mixer *handle;
	struct mixer_ctl *ctl;

	/**
	 * The range of the mixer control's raw values.
	 */
	int min_value, max_value;

	/**
	 * The number of values (i.e. channels) of the mixer control.
	 */
	unsigned num_values;

	DesiredVolume desired_volume;

public:
	TinyAlsaMixer(MixerListener &_listener,
		      const ConfigBlock &block) noexcept;

	/* virtual methods from class Mixer */
	void Open() override;
	void Close() noexcept override;
	int GetVolume() override;
	void SetVolume(unsigned volume) override;

private:
	/**
	 * Look up the mixer control.  Throws on error.
	 */
	void Setup();

	[[gnu::pure]]
	int RawToPercent(long raw) const noexcept {
		return std::lround((raw - min_value) * 100.0 /
				   (max_value - min_value));
	}

	[[gnu::pure]]
	long PercentToRaw(unsigned percent) const noexcept {
		return min_value + std::lround((max_value - min_value) *
					       (percent / 100.0));
	}

	/**
	 * Read the current volume from the hardware.  Throws on
	 * error.
	 */
	int GetPercentVolume() const;
};

TinyAlsaMixer::TinyAlsaMixer(MixerListener &_listener,
			     const ConfigBlock &block) noexcept
	:Mixer(tinyalsa_mixer_plugin, _listener),
	 card(block.GetBlockValue("card", 0U)),
	 control(block.GetBlockValue("mixer_control", DEFAULT_CONTROL)),
	 index(block.GetBlockValue("mixer_index", 0U))
{
}

static Mixer *
tinyalsa_mixer_init([[maybe_unused]] EventLoop &event_loop,
		    [[maybe_unused]] AudioOutput &ao,
		    MixerListener &listener,
		    const ConfigBlock &block)
{
	return new TinyAlsaMixer(listener, block);
}

/**
 * Build a human-readable list of all integer mixer controls (for
 * error messages).
 */
static std::string
ListIntegerControls(struct mixer &mixer)
{
	std::string result;

	const unsigned n = mixer_get_num_ctls(&mixer);
	for (unsigned i = 0; i < n; ++i) {
		const struct mixer_ctl *ctl = mixer_get_ctl(&mixer, i);
		if (ctl == nullptr ||
		    mixer_ctl_get_type(ctl) != MIXER_CTL_TYPE_INT)
			continue;

		if (!result.empty())
			result += ", ";

		fmt::format_to(std::back_inserter(result), "{:?}",
			       mixer_ctl_get_name(ctl));
	}

	if (result.empty())
		result = "none";

	return result;
}

inline void
TinyAlsaMixer::Setup()
{
	ctl = mixer_get_ctl_by_name_and_index(handle, control.c_str(), index);
	if (ctl == nullptr)
		throw FmtRuntimeError("No such mixer control {:?},{} on card {} (available: {})",
				      control, index, card,
				      ListIntegerControls(*handle));

	if (mixer_ctl_get_type(ctl) != MIXER_CTL_TYPE_INT)
		throw FmtRuntimeError("Mixer control {:?} is not an integer control",
				      control);

	min_value = mixer_ctl_get_range_min(ctl);
	max_value = mixer_ctl_get_range_max(ctl);
	if (max_value <= min_value)
		throw FmtRuntimeError("Mixer control {:?} has an empty range",
				      control);

	num_values = mixer_ctl_get_num_values(ctl);
	if (num_values == 0 || num_values > MAX_VALUES)
		throw FmtRuntimeError("Mixer control {:?} has an unsupported number of values: {}",
				      control, num_values);
}

void
TinyAlsaMixer::Open()
{
	desired_volume.Clear();

	handle = mixer_open(card);
	if (handle == nullptr)
		throw FmtRuntimeError("Failed to open tinyalsa mixer of card {}",
				      card);

	try {
		Setup();
	} catch (...) {
		mixer_close(handle);
		throw;
	}
}

void
TinyAlsaMixer::Close() noexcept
{
	assert(handle != nullptr);

	mixer_close(handle);
}

int
TinyAlsaMixer::GetPercentVolume() const
{
	/* use the first value (e.g. the left channel) */
	long value;
	if (mixer_ctl_get_array(ctl, &value, 1) < 0)
		throw MakeErrno("Failed to read tinyalsa mixer control");

	return RawToPercent(value);
}

int
TinyAlsaMixer::GetVolume()
{
	assert(handle != nullptr);

	/* if we're still on the value passed to SetVolume(), this
	   returns the desired volume */
	return desired_volume.Translate(GetPercentVolume());
}

void
TinyAlsaMixer::SetVolume(unsigned volume)
{
	assert(handle != nullptr);
	assert(volume <= 100);

	std::array<long, MAX_VALUES> values;
	std::fill_n(values.begin(), num_values, PercentToRaw(volume));

	if (mixer_ctl_set_array(ctl, values.data(), num_values) < 0)
		throw MakeErrno("Failed to write tinyalsa mixer control");

	desired_volume.Set(volume, GetPercentVolume());
}

const MixerPlugin tinyalsa_mixer_plugin = {
	tinyalsa_mixer_init,
	true,
};
