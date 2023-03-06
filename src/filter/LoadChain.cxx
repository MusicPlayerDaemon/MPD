// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LoadChain.hxx"
#include "Factory.hxx"
#include "Prepared.hxx"
#include "plugins/AutoConvertFilterPlugin.hxx"
#include "plugins/TwoFilters.hxx"
#include "util/IterableSplitString.hxx"

#include <string>

static void
filter_chain_append_new(std::unique_ptr<PreparedFilter> &chain,
			FilterFactory &factory,
			std::string_view template_name)
{
	/* using the AutoConvert filter just in case the specified
	   filter plugin does not support the exact input format */

	chain = ChainFilters(std::move(chain),
			     /* unfortunately, MakeFilter() wants a
				null-terminated string, so we need to
				copy it here */
			     autoconvert_filter_new(factory.MakeFilter(std::string(template_name).c_str())),
			     template_name);
}

void
filter_chain_parse(std::unique_ptr<PreparedFilter> &chain,
		   FilterFactory &factory,
		   const char *spec)
{
	for (const std::string_view i : IterableSplitString(spec, ',')) {
		if (i.empty())
			continue;

		filter_chain_append_new(chain, factory, i);
	}
}
