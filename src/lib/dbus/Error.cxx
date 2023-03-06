// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Error.hxx"
#include "lib/fmt/RuntimeError.hxx"

void
ODBus::Error::Throw(const char *prefix) const
{
	throw FmtRuntimeError("{}: {}", prefix, GetMessage());
}

void
ODBus::Error::CheckThrow(const char *prefix) const
{
	if (*this)
		Throw(prefix);
}
