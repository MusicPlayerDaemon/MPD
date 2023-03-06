// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Error.hxx"

#include <stdarg.h>
#include <alsa/error.h>

namespace Alsa {

ErrorCategory error_category;

std::string
ErrorCategory::message(int condition) const
{
	return snd_strerror(condition);
}

} // namespace Alsa
