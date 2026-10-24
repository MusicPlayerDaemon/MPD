// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Context.hxx"
#include "Error.hxx"
#include "Url.hxx"

extern "C" {
#include <nfsc/libnfs.h>
}

#include <cassert>
#include <stdexcept>

struct nfs_context;

NfsContext::NfsContext(Create)
	:value(nfs_init_context())
{
	if (value == nullptr)
		throw std::runtime_error{"nfs_init_context() failed"};
}

void
NfsContext::reset() noexcept
{
	if (value != nullptr)
		nfs_destroy_context(value);
}

NfsUrl
NfsContext::ParseUrlFull(const char *url) const
{
	assert(value != nullptr);

	auto *p = nfs_parse_url_full(value, url);
	if (p == nullptr)
		throw NfsClientError(value, "nfs_parse_url_full() failed");

	return NfsUrl{p};
}

NfsUrl
NfsContext::ParseUrlDir(const char *url) const
{
	assert(value != nullptr);

	auto *p = nfs_parse_url_dir(value, url);
	if (p == nullptr)
		throw NfsClientError(value, "nfs_parse_url_dir() failed");

	return NfsUrl{p};
}
