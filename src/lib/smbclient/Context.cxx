// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Context.hxx"
#include "system/Error.hxx"

#include <cerrno>

#include <string.h>

Mutex SmbclientContext::global_mutex;

static void
mpd_smbc_get_auth_data([[maybe_unused]] const char *srv,
		       [[maybe_unused]] const char *shr,
		       char *wg, [[maybe_unused]] int wglen,
		       char *un, [[maybe_unused]] int unlen,
		       char *pw, [[maybe_unused]] int pwlen)
{
	// TODO: implement
	strcpy(wg, "WORKGROUP");
	strcpy(un, "");
	strcpy(pw, "");
}

SmbclientContext
SmbclientContext::New()
{
	SMBCCTX *ctx;

	{
		const std::scoped_lock protect{global_mutex};
		ctx = smbc_new_context();
	}

	if (ctx == nullptr)
		throw MakeErrno("smbc_new_context() failed");

	constexpr int debug = 0;
	smbc_setDebug(ctx, debug);
	smbc_setFunctionAuthData(ctx, mpd_smbc_get_auth_data);

	SMBCCTX *ctx2 = smbc_init_context(ctx);
	if (ctx2 == nullptr) {
		int e = errno;
		smbc_free_context(ctx, 1);
		throw MakeErrno(e, "smbc_init_context() failed");
	}

	return SmbclientContext(ctx2);
}
