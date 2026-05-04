// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstddef>
#include <cstdint> // for SIZE_MAX

namespace Curl {

struct StringOptions {
	std::size_t max_size = SIZE_MAX;
};

} // namespace Curl
