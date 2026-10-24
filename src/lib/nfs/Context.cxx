// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Context.hxx"
#include "Error.hxx"

extern "C" {
#include <nfsc/libnfs.h>
}

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
