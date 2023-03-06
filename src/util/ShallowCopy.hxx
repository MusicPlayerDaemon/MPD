// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef SHALLOW_COPY_HXX
#define SHALLOW_COPY_HXX

/**
 * A tag for overloading copying constructors, telling them to make
 * shallow copies of source data (e.g. copy pointers instead of
 * duplicating the referenced objects).
 */
struct ShallowCopy {};

#endif
