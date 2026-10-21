// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PcmCard.hxx"
#include "util/ScopeExit.hxx"

#include <alsa/asoundlib.h>

using std::string_view_literals::operator""sv;

namespace Alsa {

/**
 * Limit the recursion depth to protect against loops in the ALSA
 * configuration.
 */
static constexpr unsigned MAX_DEPTH = 16;

static std::string
FindPcmCard(snd_config_t *top, const char *pcm_name, unsigned depth);

static std::string
GetCardControl(snd_config_t *card)
{
	long index;
	if (snd_config_get_integer(card, &index) >= 0)
		return "hw:" + std::to_string(index);

	const char *name;
	if (snd_config_get_string(card, &name) >= 0)
		return std::string{"hw:"} + name;

	return {};
}

/**
 * @param pcm an (expanded) PCM definition
 */
static std::string
FindPcmCard(snd_config_t *top, snd_config_t *pcm, unsigned depth)
{
	if (++depth > MAX_DEPTH)
		return {};

	snd_config_t *node;
	const char *type;
	if (snd_config_search(pcm, "type", &node) >= 0 &&
	    snd_config_get_string(node, &type) >= 0 &&
	    type == "hw"sv) {
		if (snd_config_search(pcm, "card", &node) < 0)
			return {};

		return GetCardControl(node);
	}

	/* follow the slave PCM of plugins such as "plug", "dmix" or
	   "empty", and the playback PCM of "asym" */
	for (const char *key : {"slave.pcm", "playback.pcm"}) {
		if (snd_config_search(pcm, key, &node) < 0)
			continue;

		/* this may be a reference to another PCM definition
		   or an inline definition */
		const char *name;
		if (snd_config_get_string(node, &name) >= 0)
			return FindPcmCard(top, name, depth);

		return FindPcmCard(top, node, depth);
	}

	return {};
}

static std::string
FindPcmCard(snd_config_t *top, const char *pcm_name, unsigned depth)
{
	snd_config_t *pcm;
	if (snd_config_search_definition(top, "pcm", pcm_name, &pcm) < 0)
		return {};

	AtScopeExit(pcm) { snd_config_delete(pcm); };

	return FindPcmCard(top, pcm, depth);
}

std::string
FindPcmCard(snd_config_t *top, const char *pcm_name)
{
	return FindPcmCard(top, pcm_name, 0);
}

std::string
FindPcmCard(const char *pcm_name)
{
	snd_config_t *top;
	if (snd_config_update_ref(&top) < 0)
		return {};

	AtScopeExit(top) { snd_config_unref(top); };

	return FindPcmCard(top, pcm_name);
}

} // namespace Alsa
