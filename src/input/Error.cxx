// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Error.hxx"
#include "system/Error.hxx"
#include "config.h"

#ifdef ENABLE_CURL
#include "lib/curl/HttpStatusError.hxx"
#endif

#ifdef ENABLE_NFS
#include "lib/nfs/Error.hxx"
#include <nfsc/libnfs-raw-nfs.h>
#endif

#include <utility>

bool
IsFileNotFound(std::exception_ptr ep) noexcept
{
	try {
		std::rethrow_exception(std::move(ep));
	} catch (const std::system_error &e) {
		return IsFileNotFound(e);
#ifdef ENABLE_CURL
	} catch (const HttpStatusError &e) {
		return e.GetStatus() == 404;
#endif
#ifdef ENABLE_NFS
	} catch (const NfsClientError &e) {
		return e.GetCode() == NFS3ERR_NOENT;
#endif
	} catch (...) {
	}

	return false;
}

