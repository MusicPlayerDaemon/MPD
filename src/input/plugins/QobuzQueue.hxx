/*
 * Copyright 2003-2018 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef QOBUZ_QUEUE_HXX
#define QOBUZ_QUEUE_HXX

#include "Compiler.h"
#include "check.h"
#include "QobuzRequest.hxx"

#include <mutex>
#include <condition_variable>
#include <exception>

struct Album;
struct Playlist;
struct RangeArg;
class Client;

class QobuzQueue : public QobuzHandler {
	static constexpr int DEFAULT_TIMEOUT = 30; // seconds
	std::mutex mutex;
	std::condition_variable cond;
	std::exception_ptr exception_ptr;

public:
	QobuzQueue();

	~QobuzQueue() noexcept;

	bool Add(Client &client, const char *uri, const RangeArg &range);

private: // override QobuzHandler
	void OnQobuzSuccess() noexcept override;
	void OnQobuzError(std::exception_ptr error) noexcept override;
};

#endif
