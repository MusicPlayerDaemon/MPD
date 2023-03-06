// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ARCHIVE_VISITOR_HXX
#define MPD_ARCHIVE_VISITOR_HXX

class ArchiveVisitor {
public:
	virtual void VisitArchiveEntry(const char *path_utf8) = 0;
};

#endif
