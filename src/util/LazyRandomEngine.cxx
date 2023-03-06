// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LazyRandomEngine.hxx"

void LazyRandomEngine::AutoCreate() {
	if (engine)
		return;

	std::random_device rd;
	engine.emplace(rd());
}
