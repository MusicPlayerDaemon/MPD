#include "config.h"
#include "Queue.hxx"
#include "Song.hxx"
#include "Directory.hxx"
#include "util/Macros.hxx"

#include <glib.h>

Directory detached_root;

Directory::Directory() {}
Directory::~Directory() {}

Song *
Song::DupDetached() const
{
	return const_cast<Song *>(this);
}

void
Song::Free()
{
}

gcc_unused
static void
dump_order(const struct queue *queue)
{
	g_printerr("queue length=%u, order:\n", queue->GetLength());
	for (unsigned i = 0; i < queue->GetLength(); ++i)
		g_printerr("  [%u] -> %u (prio=%u)\n", i, queue->order[i],
			   queue->items[queue->order[i]].priority);
}

static void
check_descending_priority(const struct queue *queue,
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

int
main(gcc_unused int argc, gcc_unused char **argv)
{
	static Song songs[16];

	struct queue queue(32);

	for (unsigned i = 0; i < ARRAY_SIZE(songs); ++i)
		queue.Append(&songs[i], 0);

	assert(queue.GetLength() == ARRAY_SIZE(songs));

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

	for (unsigned i = 8; i < ARRAY_SIZE(songs); ++i) {
		assert(queue.PositionToOrder(i) >= 4);
	}

	/* priority=50 one more item */

	queue.SetPriorityRange(15, 16, 50, -1);
	check_descending_priority(&queue, 0);

	assert(queue.PositionToOrder(15) == 0);

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

	assert(queue.PositionToOrder(3) == 1);
	assert(queue.PositionToOrder(15) == 0);

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
	assert(queue.items[a_position].priority == 10);
	queue.SetPriority(a_position, 20, current_order);

	current_order = queue.PositionToOrder(current_position);
	assert(current_order == 3);

	a_order = queue.PositionToOrder(a_position);
	assert(a_order == 4);

	check_descending_priority(&queue, current_order + 1);

	/* priority=70 for one of the last items; must be inserted
	   right after the current song, before the priority=20 one we
	   just created */

	unsigned b_order = 10;
	unsigned b_position = queue.OrderToPosition(b_order);
	assert(queue.items[b_position].priority == 0);
	queue.SetPriority(b_position, 70, current_order);

	current_order = queue.PositionToOrder(current_position);
	assert(current_order == 3);

	b_order = queue.PositionToOrder(b_position);
	assert(b_order == 4);

	check_descending_priority(&queue, current_order + 1);

	/* priority=60 for the old prio50 item; must not be moved,
	   because it's before the current song, and it's status
	   hasn't changed (it was already higher before) */

	unsigned c_order = 0;
	unsigned c_position = queue.OrderToPosition(c_order);
	assert(queue.items[c_position].priority == 50);
	queue.SetPriority(c_position, 60, current_order);

	current_order = queue.PositionToOrder(current_position);
	assert(current_order == 3);

	c_order = queue.PositionToOrder(c_position);
	assert(c_order == 0);

	/* move the prio=20 item back */

	a_order = queue.PositionToOrder(a_position);
	assert(a_order == 5);
	assert(queue.items[a_position].priority == 20);
	queue.SetPriority(a_position, 5, current_order);

	current_order = queue.PositionToOrder(current_position);
	assert(current_order == 3);

	a_order = queue.PositionToOrder(a_position);
	assert(a_order == 6);
}
