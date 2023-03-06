// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Client.hxx"
#include "protocol/Ack.hxx"
#include "fs/Path.hxx"
#include "fs/FileInfo.hxx"

void
Client::AllowFile(Path path_fs) const
{
#ifdef _WIN32
	(void)path_fs;

	throw ProtocolError(ACK_ERROR_PERMISSION, "Access denied");
#else
	if (uid >= 0 && (uid_t)uid == geteuid())
		/* always allow access if user runs his own MPD
		   instance */
		return;

	if (uid < 0)
		/* unauthenticated client */
		throw ProtocolError(ACK_ERROR_PERMISSION, "Access denied");

	const FileInfo fi(path_fs);

	if (fi.GetUid() != (uid_t)uid && (fi.GetMode() & 0444) != 0444)
		/* client is not owner */
		throw ProtocolError(ACK_ERROR_PERMISSION, "Access denied");
#endif
}
