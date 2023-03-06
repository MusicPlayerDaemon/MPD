// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <map>
#include <string>

namespace Curl {

using Headers = std::multimap<std::string, std::string, std::less<>>;

} // namespace Curl
