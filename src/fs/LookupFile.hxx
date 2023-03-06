// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_LOOKUP_FILE_HXX
#define MPD_LOOKUP_FILE_HXX

#include "AllocatedPath.hxx"

struct ArchiveLookupResult {
	AllocatedPath archive = nullptr;
	AllocatedPath inside = nullptr;

	operator bool() const noexcept {
		return !archive.IsNull();
	}
};

/**
 *
 * archive_lookup is used to determine if part of pathname refers to an regular
 * file (archive). If so then its also used to split pathname into archive file
 * and path used to locate file in archive.
 * How it works:
 * We do stat of the parent of input pathname as long as we find an regular file
 * Normally this should never happen. When routine returns true pathname modified
 * and split into archive and inpath. Otherwise nothing happens
 *
 * For example:
 *
 * /music/path/Talco.zip/Talco - Combat Circus/12 - A la pachenka.mp3
 * is split into archive:	/music/path/Talco.zip
 * inarchive pathname:		Talco - Combat Circus/12 - A la pachenka.mp3
 *
 * Throws on error.
 */
ArchiveLookupResult
LookupFile(Path pathname);

#endif

