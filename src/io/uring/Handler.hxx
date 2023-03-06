// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <exception>

struct statx;
class UniqueFileDescriptor;

namespace Uring {

class OpenStatHandler {
public:
	virtual void OnOpenStat(UniqueFileDescriptor fd,
				struct statx &st) noexcept = 0;
	virtual void OnOpenStatError(std::exception_ptr e) noexcept = 0;
};

} // namespace Uring
