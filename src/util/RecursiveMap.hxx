/*
 * Copyright 2019 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RECURSIVE_MAP_HXX
#define RECURSIVE_MAP_HXX

#include "lib/icu/Collate.hxx"

#include <map>
#include <functional>

/**
 * A #std::map which contains instances of itself.
 */
template<typename Key, typename Compare>
class RecursiveMap : public std::map<Key, RecursiveMap<Key, Compare>, Compare> {};

struct less_string_case_insensitive : public std::binary_function<std::string,
																  std::string, 
																  bool>
{
	bool operator()(const std::string& l, const std::string& r) const
	{
		return IcuCollate(std::string_view(l.data(), l.size()),
						  std::string_view(r.data(), r.size())) < 0;
	}
};

using RecursiveStringMapCS = RecursiveMap<std::string, std::less<std::string>>;
using RecursiveStringMapCI = RecursiveMap<std::string, less_string_case_insensitive>;

#endif
