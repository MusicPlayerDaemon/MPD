// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef RECURSIVE_MAP_HXX
#define RECURSIVE_MAP_HXX

#include <map>

/**
 * A #std::map which contains instances of itself.
 */
template<typename Key>
class RecursiveMap : public std::map<Key, RecursiveMap<Key>> {};

#endif
