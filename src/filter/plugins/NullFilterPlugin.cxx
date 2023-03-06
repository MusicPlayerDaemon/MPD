// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * This filter plugin does nothing.  That is not quite useful, except
 * for testing the filter core, or as a template for new filter
 * plugins.
 */

#include "NullFilterPlugin.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/NullFilter.hxx"
#include "filter/Prepared.hxx"

class PreparedNullFilter final : public PreparedFilter {
public:
	std::unique_ptr<Filter> Open(AudioFormat &af) override {
		return std::make_unique<NullFilter>(af);
	}
};

static std::unique_ptr<PreparedFilter>
null_filter_init([[maybe_unused]] const ConfigBlock &block)
{
	return std::make_unique<PreparedNullFilter>();
}

const FilterPlugin null_filter_plugin = {
	"null",
	null_filter_init,
};
