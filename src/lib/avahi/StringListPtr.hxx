// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <avahi-common/strlst.h>

#include <memory>

namespace Avahi {

struct StringListDeleter {
	void operator()(AvahiStringList *b) noexcept {
		avahi_string_list_free(b);
	}
};

using StringListPtr = std::unique_ptr<AvahiStringList,
				      StringListDeleter>;

} // namespace Avahi
