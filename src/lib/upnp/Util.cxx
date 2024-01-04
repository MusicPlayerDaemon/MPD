// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Util.hxx"

/** Get rid of white space at both ends */
void
trimstring(std::string &s, const char *ws) noexcept
{
	auto pos = s.find_first_not_of(ws);
	if (pos == std::string::npos) {
		s.clear();
		return;
	}
	s.erase(0, pos);

	pos = s.find_last_not_of(ws);
	if (pos != std::string::npos && pos != s.length()-1)
		s.erase(pos + 1);
}

static void
path_catslash(std::string &s) noexcept
{
	if (s.empty() || s.back() != '/')
		s += '/';
}

std::string
path_getfather(const std::string_view s) noexcept
{
	std::string father{s};

	// ??
	if (father.empty())
		return "./";

	if (father.back() == '/') {
		// Input ends with /. Strip it, handle special case for root
		if (father.length() == 1)
			return father;
		father.erase(father.length()-1);
	}

	auto slp = father.rfind('/');
	if (slp == std::string::npos)
		return "./";

	father.erase(slp);
	path_catslash(father);
	return father;
}
