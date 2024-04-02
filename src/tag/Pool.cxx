// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Pool.hxx"
#include "Item.hxx"
#include "util/Cast.hxx"
#include "util/djb_hash.hxx"
#include "util/IntrusiveHashSet.hxx"
#include "util/SpanCast.hxx"
#include "util/VarSize.hxx"

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>

Mutex tag_pool_lock;

struct TagPoolKey {
	std::string_view value;
	TagType type;

	friend constexpr auto operator<=>(const TagPoolKey &,
					  const TagPoolKey &) noexcept = default;

	struct Hash {
		[[gnu::pure]]
		std::size_t operator()(const TagPoolKey &key) const noexcept {
			return djb_hash(AsBytes(key.value)) ^ key.type;
		}
	};
};

struct TagPoolItem {
	IntrusiveHashSetHook<IntrusiveHookMode::NORMAL> hash_set_hook;
	uint8_t ref = 1;
	TagItem item;

	static constexpr unsigned MAX_REF = std::numeric_limits<decltype(ref)>::max();

	TagPoolItem(TagType type,
		    std::string_view value) noexcept {
		item.type = type;
		*std::copy(value.begin(), value.end(), item.value) = 0;
	}

	static TagPoolItem *Create(TagType type,
				   std::string_view value) noexcept;

	struct GetKey {
		[[gnu::pure]]
		constexpr TagPoolKey operator()(const TagItem &i) const noexcept {
			return { i.value, i.type };
		}

		[[gnu::pure]]
		constexpr TagPoolKey operator()(const TagPoolItem &i) const noexcept {
			return operator()(i.item);
		}
	};

	struct CanIncrementRef {
		constexpr bool operator()(const TagPoolItem &i) const noexcept {
			return i.ref < MAX_REF;
		}
	};
};

TagPoolItem *
TagPoolItem::Create(TagType type,
		    std::string_view value) noexcept
{
	TagPoolItem *dummy;
	return NewVarSize<TagPoolItem>(sizeof(dummy->item.value),
				       value.size() + 1,
				       type,
				       value);
}

static IntrusiveHashSet<TagPoolItem, 16384,
	IntrusiveHashSetOperators<TagPoolItem, TagPoolItem::GetKey,
				  TagPoolKey::Hash,
				  std::equal_to<TagPoolKey>>,
	IntrusiveHashSetMemberHookTraits<&TagPoolItem::hash_set_hook>,
	IntrusiveHashSetOptions{.zero_initialized = true}> tag_pool;

static constexpr TagPoolItem *
TagItemToPoolItem(TagItem *item) noexcept
{
	return &ContainerCast(*item, &TagPoolItem::item);
}

TagItem *
tag_pool_get_item(TagType type, std::string_view value) noexcept
{
	const auto [position, inserted] =
		tag_pool.insert_check_if(TagPoolKey{value, type},
					 TagPoolItem::CanIncrementRef{});

	if (inserted) {
		auto *pool_item = TagPoolItem::Create(type, value);
		tag_pool.insert_commit(position, *pool_item);
		return &pool_item->item;
	} else {
		++position->ref;
		return &position->item;
	}
}

TagItem *
tag_pool_dup_item(TagItem *item) noexcept
{
	TagPoolItem *pool_item = TagItemToPoolItem(item);

	assert(pool_item->ref > 0);

	if (pool_item->ref < TagPoolItem::MAX_REF) {
		++pool_item->ref;
		return item;
	} else {
		/* the reference counter overflows above MAX_REF;
		   obtain a reference to a different TagPoolItem which
		   isn't yet "full" */
		return tag_pool_get_item(item->type, item->value);
	}
}

void
tag_pool_put_item(TagItem *item) noexcept
{
	TagPoolItem *const pool_item = TagItemToPoolItem(item);
	assert(pool_item->ref > 0);
	--pool_item->ref;

	if (pool_item->ref > 0)
		return;

	tag_pool.erase(tag_pool.iterator_to(*pool_item));
	DeleteVarSize(pool_item);
}
