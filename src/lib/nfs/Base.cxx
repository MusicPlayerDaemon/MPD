// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Base.hxx"
#include "thread/Mutex.hxx"
#include "util/StringCompare.hxx"

#include <cassert>
#include <forward_list>
#include <string>

namespace {

struct NfsBase {
	const void *const owner;
	const std::string server, export_name;

	[[nodiscard]]
	NfsBase(const void *_owner,
		std::string_view _server, std::string_view _export_name) noexcept
		:owner(_owner), server(_server), export_name(_export_name) {}

	[[gnu::pure]]
	const char *CheckPath(const char *path) const noexcept {
		const char *result = StringAfterPrefix(path, export_name);
		if (result != nullptr && (*result == 0 || *result == '/'))
			return result;

		return nullptr;
	}

	[[gnu::pure]]
	const char *Check(std::string_view _server, const char *path) const noexcept {
		return _server == server
			? CheckPath(path)
			: nullptr;
	}
};

} // anonymous namespace

/**
 * Protects #nfs_bases.
 */
static Mutex nfs_base_mutex;

static std::forward_list<NfsBase> nfs_bases;

void
nfs_add_base(const void *owner,
	     std::string_view server, std::string_view export_name) noexcept
{
	const std::scoped_lock lock{nfs_base_mutex};
	nfs_bases.emplace_front(owner, server, export_name);
}

void
nfs_remove_base(const void *owner) noexcept
{
	const std::scoped_lock lock{nfs_base_mutex};

	[[maybe_unused]]
	const auto n_removed = nfs_bases.remove_if([owner](const NfsBase &i){
		return i.owner == owner;
	});

	assert(n_removed == 1);
}

const char *
nfs_check_base(std::string_view server, const char *path) noexcept
{
	assert(path != nullptr);

	const std::scoped_lock lock{nfs_base_mutex};

	for (const auto &i : nfs_bases)
		if (const char *result = i.Check(server, path))
			return result;

	return nullptr;
}
