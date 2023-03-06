// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef GCRYPT_INIT_HXX
#define GCRYPT_INIT_HXX

#include <gcrypt.h>

namespace Gcrypt {

inline void
Init() noexcept
{
	gcry_check_version(GCRYPT_VERSION);
}

} /* namespace Gcrypt */

#endif
