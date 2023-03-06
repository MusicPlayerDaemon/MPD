// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FILTER_LOAD_CHAIN_HXX
#define MPD_FILTER_LOAD_CHAIN_HXX

#include <memory>

class FilterFactory;
class PreparedFilter;

/**
 * Builds a filter chain from a configuration string on the form
 * "name1, name2, name3, ..." by looking up each name among the
 * configured filter sections.
 *
 * Throws on error.
 *
 * @param chain the chain to append filters on
 * @param config the global configuration to load filter definitions from
 * @param spec the filter chain specification
 */
void
filter_chain_parse(std::unique_ptr<PreparedFilter> &chain,
		   FilterFactory &factory,
		   const char *spec);

#endif
