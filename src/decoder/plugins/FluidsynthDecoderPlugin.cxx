// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FluidsynthDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "config/Block.hxx"
#include "config/Parser.hxx"
#include "pcm/CheckAudioFormat.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/Path.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "util/IterableSplitString.hxx"
#include "util/StringSplit.hxx"
#include "Log.hxx"

#include <fluidsynth.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <stdlib.h> // for strtod(), strtol()

static constexpr Domain fluidsynth_domain("fluidsynth");

static unsigned sample_rate;
static double gain = 0.0;
static bool gain_set = false;
static const char *soundfont_path;

/**
 * A FluidSynth setting from the "settings" option.
 */
struct FluidsynthSetting {
	std::string name, value;
};

static std::vector<FluidsynthSetting> fluidsynth_settings;

struct FluidSettingsDeleter {
	void operator()(fluid_settings_t *settings) const noexcept {
		delete_fluid_settings(settings);
	}
};

using UniqueFluidSettings = std::unique_ptr<fluid_settings_t,
					    FluidSettingsDeleter>;

/**
 * Check the return value of a fluid_settings_set*() call.
 */
static constexpr bool
IsFluidSettingsOk(int result) noexcept
{
#if FLUIDSYNTH_VERSION_MAJOR >= 2
	return result == FLUID_OK;
#else
	/* FluidSynth 1.x returns 1 on success */
	return result != 0;
#endif
}

/**
 * Parse the value of the "settings" option, which is a
 * space-separated list of "NAME=VALUE" pairs (like the
 * FluidSynth command-line option "-o").
 *
 * Throws on error.
 */
static std::vector<FluidsynthSetting>
ParseFluidsynthSettings(std::string_view s)
{
	std::vector<FluidsynthSetting> result;

	for (const std::string_view item : IterableSplitString(s, ' ')) {
		if (item.empty())
			continue;

		const auto [name, value] = Split(item, '=');
		if (name.empty() || value.data() == nullptr)
			throw FmtRuntimeError("Malformed FluidSynth setting {:?}, expected NAME=VALUE",
					      item);

		result.push_back({std::string{name}, std::string{value}});
	}

	return result;
}

/**
 * Apply one setting to the #fluid_settings_t object.
 *
 * Throws on error.
 */
static void
ApplyFluidsynthSetting(fluid_settings_t &settings, FluidsynthSetting &setting)
{
	char *const name = setting.name.data();
	char *const value = setting.value.data();

	int result;

	switch (fluid_settings_get_type(&settings, name)) {
	case FLUID_NUM_TYPE:
		{
			char *endptr;
			const double n = strtod(value, &endptr);
			if (endptr == value || *endptr != 0)
				throw FmtRuntimeError("FluidSynth setting {:?} is not a number: {:?}",
						      setting.name, setting.value);

			result = fluid_settings_setnum(&settings, name, n);
		}

		break;

	case FLUID_INT_TYPE:
		{
			char *endptr;
			long n = strtol(value, &endptr, 10);
			if (endptr == value || *endptr != 0) {
				/* allow "yes"/"no" for boolean
				   settings */
				try {
					n = ParseBool(value);
				} catch (...) {
					throw FmtRuntimeError("FluidSynth setting {:?} is not an integer: {:?}",
							      setting.name, setting.value);
				}
			}

			result = fluid_settings_setint(&settings, name, n);
		}

		break;

	case FLUID_STR_TYPE:
		result = fluid_settings_setstr(&settings, name, value);
		break;

	default:
		throw FmtRuntimeError("Unknown FluidSynth setting {:?}",
				      setting.name);
	}

	if (!IsFluidSettingsOk(result))
		throw FmtRuntimeError("Invalid value for FluidSynth setting {:?}: {:?}",
				      setting.name, setting.value);
}

/**
 * Create a new #fluid_settings_t object and apply all configured
 * settings.
 *
 * Throws on error.
 */
static UniqueFluidSettings
CreateFluidSettings()
{
	UniqueFluidSettings settings{new_fluid_settings()};
	if (!settings)
		throw std::runtime_error{"new_fluid_settings() failed"};

	char setting_sample_rate[] = "synth.sample-rate";
	char setting_gain[] = "synth.gain";

	fluid_settings_setnum(settings.get(), setting_sample_rate, sample_rate);
	if (gain_set) {
		fluid_settings_setnum(settings.get(), setting_gain, gain);
	}

	for (auto &i : fluidsynth_settings)
		ApplyFluidsynthSetting(*settings, i);

	return settings;
}

/**
 * Convert a fluidsynth log level to a MPD log level.
 */
static LogLevel
fluidsynth_level_to_mpd(enum fluid_log_level level)
{
	switch (level) {
	case FLUID_PANIC:
	case FLUID_ERR:
		return LogLevel::ERROR;

	case FLUID_WARN:
		return LogLevel::WARNING;

	case FLUID_INFO:
		return LogLevel::INFO;

	case FLUID_DBG:
	case LAST_LOG_LEVEL:
		return LogLevel::DEBUG;
	}

	/* invalid fluidsynth log level */
	return LogLevel::INFO;
}

/**
 * The fluidsynth logging callback.  It forwards messages to the MPD
 * logging library.
 */
static void
fluidsynth_mpd_log_function(int level,
#if FLUIDSYNTH_VERSION_MAJOR >= 2
			    const
#endif
			    char *message,
			    void *)
{
	Log(fluidsynth_level_to_mpd(fluid_log_level(level)),
	    fluidsynth_domain,
	    message);
}

static bool
fluidsynth_init(const ConfigBlock &block)
{
	sample_rate = block.GetPositiveValue("sample_rate", 48000U);
	CheckSampleRate(sample_rate);

	soundfont_path = block.GetBlockValue("soundfont",
					     "/usr/share/sounds/sf2/FluidR3_GM.sf2");

	char *endptr;
	const char *svalue = block.GetBlockValue("gain");
	if (svalue != nullptr) {
		gain = strtod(svalue, &endptr);
		if (svalue == endptr || *endptr != 0) {
			throw FmtRuntimeError(
					"fluidsynth decoder gain value not a number: {}", svalue);
		}
		gain_set = true;
	}

	fluid_set_log_function(LAST_LOG_LEVEL,
			       fluidsynth_mpd_log_function, nullptr);

	if (const auto *param = block.GetBlockParam("settings")) {
		try {
			fluidsynth_settings = ParseFluidsynthSettings(param->value);
		} catch (...) {
			std::throw_with_nested(FmtRuntimeError("Error in line {}",
							       param->line));
		}
	}

	/* create a #fluid_settings_t object now to check the
	   settings early, and to determine the effective sample rate
	   (which may have been overridden by "settings") */
	const auto settings = CreateFluidSettings();

	char setting_sample_rate[] = "synth.sample-rate";
	double effective_sample_rate;
	if (fluid_settings_getnum(settings.get(), setting_sample_rate,
				  &effective_sample_rate)
#if FLUIDSYNTH_VERSION_MAJOR >= 2
	    == FLUID_OK
#endif
	    ) {
		sample_rate = effective_sample_rate;
		CheckSampleRate(sample_rate);
	}

	return true;
}

static void
fluidsynth_finish() noexcept
{
	fluidsynth_settings.clear();
}

static void
fluidsynth_file_decode(DecoderClient &client, Path path_fs)
{
	fluid_synth_t *synth;
	fluid_player_t *player;
	int ret;

	auto np = NarrowPath(path_fs);

	/* set up fluid settings */

	const auto settings_ptr = CreateFluidSettings();
	fluid_settings_t *const settings = settings_ptr.get();

	/* create the fluid synth */

	synth = new_fluid_synth(settings);
	if (synth == nullptr)
		return;

	ret = fluid_synth_sfload(synth, soundfont_path, true);
	if (ret < 0) {
		LogWarning(fluidsynth_domain, "fluid_synth_sfload() failed");
		delete_fluid_synth(synth);
		return;
	}

	/* create the fluid player */

	player = new_fluid_player(synth);
	if (player == nullptr) {
		delete_fluid_synth(synth);
		return;
	}

	ret = fluid_player_add(player, np);
	if (ret != 0) {
		LogWarning(fluidsynth_domain, "fluid_player_add() failed");
		delete_fluid_player(player);
		delete_fluid_synth(synth);
		return;
	}

	/* start the player */

	ret = fluid_player_play(player);
	if (ret != 0) {
		LogWarning(fluidsynth_domain, "fluid_player_play() failed");
		delete_fluid_player(player);
		delete_fluid_synth(synth);
		return;
	}

	/* initialization complete - announce the audio format to the
	   MPD core */

	const AudioFormat audio_format(sample_rate, SampleFormat::S16, 2);
	client.Ready(audio_format, false, SignedSongTime::Negative());

	DecoderCommand cmd;
	while (fluid_player_get_status(player) == FLUID_PLAYER_PLAYING) {
		int16_t buffer[2048];
		const unsigned max_frames = std::size(buffer) / 2;

		/* read samples from fluidsynth and send them to the
		   MPD core */

		ret = fluid_synth_write_s16(synth, max_frames,
					    buffer, 0, 2,
					    buffer, 1, 2);
		if (ret != 0)
			break;

		cmd = client.SubmitAudio(nullptr, std::span{buffer}, 0);
		if (cmd != DecoderCommand::NONE)
			break;
	}

	/* clean up */

	fluid_player_stop(player);
	fluid_player_join(player);

	delete_fluid_player(player);
	delete_fluid_synth(synth);
}

static bool
fluidsynth_scan_file(Path path_fs,
		     [[maybe_unused]] TagHandler &handler) noexcept
{
	auto np = NarrowPath(path_fs);
	return fluid_is_midifile(np);
}

static constexpr const char *fluidsynth_suffixes[] = {
	"mid",
	nullptr
};

constexpr DecoderPlugin fluidsynth_decoder_plugin =
	DecoderPlugin("fluidsynth",
		      fluidsynth_file_decode, fluidsynth_scan_file)
	.WithInit(fluidsynth_init, fluidsynth_finish)
	.WithSuffixes(fluidsynth_suffixes);
