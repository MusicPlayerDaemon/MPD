// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DatabaseQueue.hxx"
#include "DatabaseSong.hxx"
#include "Interface.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "song/DetachedSong.hxx"

#include <functional>

static void
AddToQueue(Partition &partition, const LightSong &song)
{
	const auto *storage = partition.instance.storage;
	partition.playlist.AppendSong(partition.pc,
				      DatabaseDetachSong(storage,
							 song));
}

void
AddFromDatabase(Partition &partition, const DatabaseSelection &selection)
{
	const Database &db = partition.instance.GetDatabaseOrThrow();

	const auto f = [&](const auto &song)
		{ return AddToQueue(partition, song); };
	db.Visit(selection, f);
}
