// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

/*
 * This header contains empty struct definitiosn that are supposed to
 * select special overloads of functions/methods/constructors.
 */

/**
 * A tag for overloading copying constructors, telling them to make
 * shallow copies of source data (e.g. copy pointers instead of
 * duplicating the referenced objects).
 */
struct ShallowCopy {};
