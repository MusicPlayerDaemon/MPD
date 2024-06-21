// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ChannelMap.hxx"
#include "Error.hxx"
#include "util/Domain.hxx"
#include "util/ScopeExit.hxx"
#include "Log.hxx"

#include <algorithm> // for std::is_permutation()

static constexpr Domain alsa_output_domain{"alsa_output"};

namespace Alsa {

static constexpr unsigned chmap_flac_50[]{
	SND_CHMAP_FL, SND_CHMAP_FR, SND_CHMAP_FC, SND_CHMAP_RL, SND_CHMAP_RR
};

static constexpr unsigned chmap_alsa_50[]{
	SND_CHMAP_FL, SND_CHMAP_FR, SND_CHMAP_RL, SND_CHMAP_RR, SND_CHMAP_FC
};

static constexpr unsigned chmap_flac_51[]{
	SND_CHMAP_FL, SND_CHMAP_FR, SND_CHMAP_FC, SND_CHMAP_LFE, SND_CHMAP_RL, SND_CHMAP_RR
};

static constexpr unsigned chmap_alsa_51[]{
	SND_CHMAP_FL, SND_CHMAP_FR, SND_CHMAP_RL, SND_CHMAP_RR, SND_CHMAP_FC, SND_CHMAP_LFE
};

static constexpr unsigned chmap_flac_7[]{
	SND_CHMAP_FL, SND_CHMAP_FR, SND_CHMAP_FC, SND_CHMAP_LFE, SND_CHMAP_RC, SND_CHMAP_SL, SND_CHMAP_SR
};

static constexpr unsigned chmap_flac_8[]{
	SND_CHMAP_FL, SND_CHMAP_FR, SND_CHMAP_FC, SND_CHMAP_LFE, SND_CHMAP_RL, SND_CHMAP_RR, SND_CHMAP_SL, SND_CHMAP_SR
};

/**
 * Same as #chmap_flac_8, but with "rear R/L center" instead of "side
 * R/L".
 */
static constexpr unsigned chmap_flac_8b[]{
	SND_CHMAP_FL, SND_CHMAP_FR, SND_CHMAP_FC, SND_CHMAP_LFE, SND_CHMAP_RL, SND_CHMAP_RR, SND_CHMAP_RLC, SND_CHMAP_RRC
};

static constexpr unsigned chmap_alsa_71[]{
	SND_CHMAP_FL, SND_CHMAP_FR, SND_CHMAP_RL, SND_CHMAP_RR, SND_CHMAP_FC, SND_CHMAP_LFE, SND_CHMAP_SL, SND_CHMAP_SR
};

[[gnu::pure]]
static std::string
ChannelPositionArrayToString(const snd_pcm_chmap &chmap) noexcept
{
	std::string s;
	for (unsigned c = 0, n = chmap.channels; c < n; ++c) {
		if (!s.empty())
			s.push_back(',');
		s.append(snd_pcm_chmap_name(static_cast<enum snd_pcm_chmap_position>(chmap.pos[c])));
	}

	return s;
}

[[gnu::pure]]
static constexpr bool
ChannelMapsEqual(const unsigned *a, const unsigned *b, unsigned n) noexcept
{
	return std::equal(a, a + n, b);
}

[[gnu::pure]]
static constexpr bool
ChannelMapsEqual(const snd_pcm_chmap_query_t &a, const unsigned *b, unsigned n) noexcept
{
	if (a.map.channels != n)
		return false;

	switch (a.type) {
	case SND_CHMAP_TYPE_NONE:
		return false;

	case SND_CHMAP_TYPE_FIXED:
	case SND_CHMAP_TYPE_VAR:
	case SND_CHMAP_TYPE_PAIRED:
		return ChannelMapsEqual(a.map.pos, b, n);
	}

	return false;
}

[[gnu::pure]]
static const snd_pcm_chmap_t *
FindExactChannelMap(const snd_pcm_chmap_query_t *const*maps,
		    const unsigned *other, unsigned channels) noexcept
{
	for (; *maps != nullptr; ++maps) {
		const auto &map = **maps;
		if (ChannelMapsEqual(map, other, channels))
			return &map.map;
	}

	return nullptr;
}

[[gnu::pure]]
static constexpr bool
IsChannelMapPermutation(const unsigned *a, const unsigned *b, unsigned n) noexcept
{
	return std::is_permutation(a, a + n, b);
}

[[gnu::pure]]
static constexpr bool
IsChannelMapPermutation(const snd_pcm_chmap_query_t &a, const unsigned *b, unsigned n) noexcept
{
	if (a.map.channels != n)
		return false;

	switch (a.type) {
	case SND_CHMAP_TYPE_NONE:
	case SND_CHMAP_TYPE_FIXED:
		return false;

	case SND_CHMAP_TYPE_VAR:
		return IsChannelMapPermutation(a.map.pos, b, n);

	case SND_CHMAP_TYPE_PAIRED:
		// TODO implement check for this
		return false;
	}

	return false;
}

[[gnu::pure]]
static snd_pcm_chmap_t *
FindVarChannelMap(snd_pcm_chmap_query_t *const*maps,
		  const unsigned *other, unsigned channels) noexcept
{
	for (; *maps != nullptr; ++maps) {
		auto &map = **maps;

		if (IsChannelMapPermutation(map, other, channels))
			return &map.map;
	}

	return nullptr;
}

static bool
TrySetupChannelMap(snd_pcm_t *pcm, snd_pcm_chmap_query_t **maps,
		   unsigned channels,
		   const unsigned *want_map)
{
	assert(want_map != nullptr);

	/* find an exact channel map for MPD's map (= FLAC) */
	if (const auto *map = FindExactChannelMap(maps, want_map, channels)) {
		FmtDebug(alsa_output_domain, "Selected exact channel map {:?}",
			 ChannelPositionArrayToString(*map));

		if (int err = snd_pcm_set_chmap(pcm, map); err < 0)
			throw MakeError(err, "snd_pcm_set_chmap() failed");

		return true;
	}

	/* find a variable channel map which is a permutation of MPD's
	   and ask ALSA to swap channels */
	if (auto *map = FindVarChannelMap(maps, want_map, channels)) {
		FmtDebug(alsa_output_domain, "Selected variable channel map {:?}",
			 ChannelPositionArrayToString(*map));

		std::copy_n(want_map, channels, map->pos);

		if (int err = snd_pcm_set_chmap(pcm, map); err < 0)
			throw MakeError(err, "snd_pcm_set_chmap() failed");

		FmtDebug(alsa_output_domain, "Configured custom channel map {:?}",
			 ChannelPositionArrayToString(*map));

		return true;
	}

	return false;
}

static void
SetupChannelMap(snd_pcm_t *pcm,
		unsigned channels,
		const unsigned *flac1,
		const unsigned *flac2,
		const unsigned *alsa,
		PcmExport::Params &params)
{
	const auto maps = snd_pcm_query_chmaps(pcm);
	if (maps == nullptr) {
		LogWarning(alsa_output_domain, "No channel maps available");

		/* assume defaults and hope for the best */
		params.alsa_channel_order = true;
		return;
	}

	AtScopeExit(maps) { snd_pcm_free_chmaps(maps); };

	/* dump the available channel maps */
	for (const auto *const*m = maps; *m != nullptr; ++m) {
		const auto &map = **m;

		if (map.map.channels == channels)
			FmtDebug(alsa_output_domain, "Channel map: type={:?} {:?}",
				 snd_pcm_chmap_type_name(map.type), ChannelPositionArrayToString(map.map));
	}

	if (flac1 != nullptr && TrySetupChannelMap(pcm, maps, channels, flac1))
		return;

	if (flac2 != nullptr && TrySetupChannelMap(pcm, maps, channels, flac2))
		return;

	if (alsa != nullptr) {
		/* find an exact channel map for the (obsolete) ALSA default
		   map; this is a special case implemented by class
		   PcmExport */
		if (const auto *map = FindExactChannelMap(maps, alsa, channels)) {
			FmtDebug(alsa_output_domain, "Selected ALSA channel map {:?}",
				 ChannelPositionArrayToString(*map));

			if (int err = snd_pcm_set_chmap(pcm, map); err < 0)
				throw MakeError(err, "snd_pcm_set_chmap() failed");

			params.alsa_channel_order = true;
			return;
		}
	}

	FmtWarning(alsa_output_domain, "No matching channel map found");
}

void
SetupChannelMap(snd_pcm_t *pcm,
		unsigned channels,
		PcmExport::Params &params)
{
	switch (channels) {
	case 5:
		return SetupChannelMap(pcm, channels, chmap_flac_50, nullptr, chmap_alsa_50, params);

	case 6:
		return SetupChannelMap(pcm, channels, chmap_flac_51, nullptr, chmap_alsa_51, params);

	case 7:
		return SetupChannelMap(pcm, channels, chmap_flac_7, nullptr, nullptr, params);

	case 8:
		return SetupChannelMap(pcm, channels, chmap_flac_8, chmap_flac_8b, chmap_alsa_71, params);
	}
}

} // namespace Alsa
