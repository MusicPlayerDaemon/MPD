// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <avahi-client/publish.h>

#include <memory>

namespace Avahi {

struct EntryGroupDeleter {
	void operator()(AvahiEntryGroup *g) noexcept {
		avahi_entry_group_free(g);
	}
};

using EntryGroupPtr = std::unique_ptr<AvahiEntryGroup, EntryGroupDeleter>;

} // namespace Avahi
