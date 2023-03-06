// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_BULK_EDIT_HXX
#define MPD_BULK_EDIT_HXX

#include "Partition.hxx"

/**
 * Begin a "bulk edit" and commit it automatically.
 */
class ScopeBulkEdit {
	Partition &partition;

public:
	ScopeBulkEdit(Partition &_partition):partition(_partition) {
		partition.playlist.BeginBulk();
	}

	~ScopeBulkEdit() {
		partition.playlist.CommitBulk(partition.pc);
	}
};

#endif
