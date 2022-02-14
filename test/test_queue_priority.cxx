#include "queue/Queue.hxx"
#include "song/DetachedSong.hxx"
#include "song/LightSong.hxx"

#include <gtest/gtest.h>

#include <iterator>

Tag::Tag(const Tag &) noexcept {}
void Tag::Clear() noexcept {}

DetachedSong::operator LightSong() const noexcept
{
	return {uri.c_str(), tag};
}

static void
check_descending_priority(const Queue *queue,
			  unsigned start_order)
{
	assert(start_order < queue->GetLength());

	uint8_t last_priority = 0xff;
	for (unsigned order = start_order; order < queue->GetLength(); ++order) {
		unsigned position = queue->OrderToPosition(order);
		uint8_t priority = queue->items[position].priority;
		assert(priority <= last_priority);
		(void)last_priority;
		last_priority = priority;
	}
}

TEST(QueuePriority, Priority)
{
	DetachedSong songs[16] = {
		DetachedSong("0.ogg"),
		DetachedSong("1.ogg"),
		DetachedSong("2.ogg"),
		DetachedSong("3.ogg"),
		DetachedSong("4.ogg"),
		DetachedSong("5.ogg"),
		DetachedSong("6.ogg"),
		DetachedSong("7.ogg"),
		DetachedSong("8.ogg"),
		DetachedSong("9.ogg"),
		DetachedSong("a.ogg"),
		DetachedSong("b.ogg"),
		DetachedSong("c.ogg"),
		DetachedSong("d.ogg"),
		DetachedSong("e.ogg"),
		DetachedSong("f.ogg"),
	};

	Queue queue(32);

	for (unsigned i = 0; i < std::size(songs); ++i)
		queue.Append(DetachedSong(songs[i]), 0);

	EXPECT_EQ(unsigned(std::size(songs)), queue.GetLength());

	/* priority=10 for 4 items */

	queue.SetPriorityRange(4, 8, 10, -1);

	queue.random = true;
	queue.ShuffleOrder();
	check_descending_priority(&queue, 0);

	for (unsigned i = 0; i < 4; ++i) {
		assert(queue.PositionToOrder(i) >= 4);
	}

	for (unsigned i = 4; i < 8; ++i) {
		assert(queue.PositionToOrder(i) < 4);
	}

	for (unsigned i = 8; i < std::size(songs); ++i) {
		assert(queue.PositionToOrder(i) >= 4);
	}

	/* priority=50 one more item */

	queue.SetPriorityRange(15, 16, 50, -1);
	check_descending_priority(&queue, 0);

	EXPECT_EQ(0u, queue.PositionToOrder(15));

	for (unsigned i = 0; i < 4; ++i) {
		assert(queue.PositionToOrder(i) >= 4);
	}

	for (unsigned i = 4; i < 8; ++i) {
		assert(queue.PositionToOrder(i) >= 1 &&
		       queue.PositionToOrder(i) < 5);
	}

	for (unsigned i = 8; i < 15; ++i) {
		assert(queue.PositionToOrder(i) >= 5);
	}

	/* priority=20 for one of the 4 priority=10 items */

	queue.SetPriorityRange(3, 4, 20, -1);
	check_descending_priority(&queue, 0);

	EXPECT_EQ(1u, queue.PositionToOrder(3));
	EXPECT_EQ(0u, queue.PositionToOrder(15));

	for (unsigned i = 0; i < 3; ++i) {
		assert(queue.PositionToOrder(i) >= 5);
	}

	for (unsigned i = 4; i < 8; ++i) {
		assert(queue.PositionToOrder(i) >= 2 &&
		       queue.PositionToOrder(i) < 6);
	}

	for (unsigned i = 8; i < 15; ++i) {
		assert(queue.PositionToOrder(i) >= 6);
	}

	/* priority=20 for another one of the 4 priority=10 items;
	   pass "after_order" (with priority=10) and see if it's moved
	   after that one */

	unsigned current_order = 4;
	unsigned current_position =
		queue.OrderToPosition(current_order);

	unsigned a_order = 3;
	unsigned a_position = queue.OrderToPosition(a_order);
	EXPECT_EQ(10u, unsigned(queue.items[a_position].priority));
	queue.SetPriority(a_position, 20, current_order);

	current_order = queue.PositionToOrder(current_position);
	EXPECT_EQ(3u, current_order);

	a_order = queue.PositionToOrder(a_position);
	EXPECT_EQ(4u, a_order);

	check_descending_priority(&queue, current_order + 1);

	/* priority=70 for one of the last items; must be inserted
	   right after the current song, before the priority=20 one we
	   just created */

	unsigned b_order = 10;
	unsigned b_position = queue.OrderToPosition(b_order);
	EXPECT_EQ(0u, unsigned(queue.items[b_position].priority));
	queue.SetPriority(b_position, 70, current_order);

	current_order = queue.PositionToOrder(current_position);
	EXPECT_EQ(3u, current_order);

	b_order = queue.PositionToOrder(b_position);
	EXPECT_EQ(4u, b_order);

	check_descending_priority(&queue, current_order + 1);

	/* move the prio=20 item back */

	a_order = queue.PositionToOrder(a_position);
	EXPECT_EQ(5u, a_order);
	EXPECT_EQ(20u, unsigned(queue.items[a_position].priority));
	queue.SetPriority(a_position, 5, current_order);

	current_order = queue.PositionToOrder(current_position);
	EXPECT_EQ(3u, current_order);

	a_order = queue.PositionToOrder(a_position);
	EXPECT_EQ(6u, a_order);
}
