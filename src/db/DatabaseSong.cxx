// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DatabaseSong.hxx"
#include "Interface.hxx"
#include "song/DetachedSong.hxx"
#include "song/LightSong.hxx"
#include "storage/StorageInterface.hxx"
#include "util/ScopeExit.hxx"
#include "util/UriExtract.hxx"
#include "util/UriRelative.hxx"

#include <cassert>

DetachedSong
DatabaseDetachSong(const Storage *storage, const LightSong &song) noexcept
{
	DetachedSong detached(song);
	assert(detached.IsInDatabase());

	if (storage != nullptr) {
		if (!detached.HasRealURI()) {
			const auto uri = song.GetURI();
			detached.SetRealURI(storage->MapUTF8(uri));
		} else if (uri_is_relative_path(detached.GetRealURI())) {
			/* if the "RealURI" is relative, translate it
			   using the song's "URI" attribute, because
			   it's assumed to be relative to it */
			const auto real_uri =
				uri_apply_relative(detached.GetRealURI(),
						   song.GetURI());
			detached.SetRealURI(storage->MapUTF8(real_uri));
		}
	}

	return detached;
}

DetachedSong
DatabaseDetachSong(const Database &db, const Storage *storage, const char *uri)
{
	const LightSong *tmp = db.GetSong(uri);
	assert(tmp != nullptr);

	AtScopeExit(&db, tmp) { db.ReturnSong(tmp); };

	return DatabaseDetachSong(storage, *tmp);
}
