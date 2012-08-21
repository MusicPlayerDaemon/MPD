#include "queue.h"
#include "song.h"

void
song_free(G_GNUC_UNUSED struct song *song)
{
}

G_GNUC_UNUSED
static void
dump_order(const struct queue *queue)
{
	g_printerr("queue length=%u, order:\n", queue_length(queue));
	for (unsigned i = 0; i < queue_length(queue); ++i)
		g_printerr("  [%u] -> %u (prio=%u)\n", i, queue->order[i],
			   queue->items[queue->order[i]].priority);
}

static void
check_descending_priority(G_GNUC_UNUSED const struct queue *queue,
			 unsigned start_order)
{
	assert(start_order < queue_length(queue));

	uint8_t last_priority = 0xff;
	for (unsigned order = start_order; order < queue_length(queue); ++order) {
		unsigned position = queue_order_to_position(queue, order);
		uint8_t priority = queue->items[position].priority;
		assert(priority <= last_priority);
		(void)last_priority;
		last_priority = priority;
	}
}

int
main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	struct song songs[16];

	struct queue queue;
	queue_init(&queue, 32);

	for (unsigned i = 0; i < G_N_ELEMENTS(songs); ++i)
		queue_append(&queue, &songs[i], 0);

	assert(queue_length(&queue) == G_N_ELEMENTS(songs));

	/* priority=10 for 4 items */

	queue_set_priority_range(&queue, 4, 8, 10, -1);

	queue.random = true;
	queue_shuffle_order(&queue);
	check_descending_priority(&queue, 0);

	for (unsigned i = 0; i < 4; ++i) {
		assert(queue_position_to_order(&queue, i) >= 4);
	}

	for (unsigned i = 4; i < 8; ++i) {
		assert(queue_position_to_order(&queue, i) < 4);
	}

	for (unsigned i = 8; i < G_N_ELEMENTS(songs); ++i) {
		assert(queue_position_to_order(&queue, i) >= 4);
	}

	/* priority=50 one more item */

	queue_set_priority_range(&queue, 15, 16, 50, -1);
	check_descending_priority(&queue, 0);

	assert(queue_position_to_order(&queue, 15) == 0);

	for (unsigned i = 0; i < 4; ++i) {
		assert(queue_position_to_order(&queue, i) >= 4);
	}

	for (unsigned i = 4; i < 8; ++i) {
		assert(queue_position_to_order(&queue, i) >= 1 &&
		       queue_position_to_order(&queue, i) < 5);
	}

	for (unsigned i = 8; i < 15; ++i) {
		assert(queue_position_to_order(&queue, i) >= 5);
	}

	/* priority=20 for one of the 4 priority=10 items */

	queue_set_priority_range(&queue, 3, 4, 20, -1);
	check_descending_priority(&queue, 0);

	assert(queue_position_to_order(&queue, 3) == 1);
	assert(queue_position_to_order(&queue, 15) == 0);

	for (unsigned i = 0; i < 3; ++i) {
		assert(queue_position_to_order(&queue, i) >= 5);
	}

	for (unsigned i = 4; i < 8; ++i) {
		assert(queue_position_to_order(&queue, i) >= 2 &&
		       queue_position_to_order(&queue, i) < 6);
	}

	for (unsigned i = 8; i < 15; ++i) {
		assert(queue_position_to_order(&queue, i) >= 6);
	}

	/* priority=20 for another one of the 4 priority=10 items;
	   pass "after_order" (with priority=10) and see if it's moved
	   after that one */

	unsigned current_order = 4;
	unsigned current_position =
		queue_order_to_position(&queue, current_order);

	unsigned a_order = 3;
	unsigned a_position = queue_order_to_position(&queue, a_order);
	assert(queue.items[a_position].priority == 10);
	queue_set_priority(&queue, a_position, 20, current_order);

	current_order = queue_position_to_order(&queue, current_position);
	assert(current_order == 3);

	a_order = queue_position_to_order(&queue, a_position);
	assert(a_order == 4);

	check_descending_priority(&queue, current_order + 1);

	/* priority=70 for one of the last items; must be inserted
	   right after the current song, before the priority=20 one we
	   just created */

	unsigned b_order = 10;
	unsigned b_position = queue_order_to_position(&queue, b_order);
	assert(queue.items[b_position].priority == 0);
	queue_set_priority(&queue, b_position, 70, current_order);

	current_order = queue_position_to_order(&queue, current_position);
	assert(current_order == 3);

	b_order = queue_position_to_order(&queue, b_position);
	assert(b_order == 4);

	check_descending_priority(&queue, current_order + 1);

	/* priority=60 for the old prio50 item; must not be moved,
	   because it's before the current song, and it's status
	   hasn't changed (it was already higher before) */

	unsigned c_order = 0;
	unsigned c_position = queue_order_to_position(&queue, c_order);
	assert(queue.items[c_position].priority == 50);
	queue_set_priority(&queue, c_position, 60, current_order);

	current_order = queue_position_to_order(&queue, current_position);
	assert(current_order == 3);

	c_order = queue_position_to_order(&queue, c_position);
	assert(c_order == 0);

	/* move the prio=20 item back */

	a_order = queue_position_to_order(&queue, a_position);
	assert(a_order == 5);
	assert(queue.items[a_position].priority == 20);
	queue_set_priority(&queue, a_position, 5, current_order);


	current_order = queue_position_to_order(&queue, current_position);
	assert(current_order == 3);

	a_order = queue_position_to_order(&queue, a_position);
	assert(a_order == 6);
}
