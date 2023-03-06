// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * This library sends information about songs in the queue to the
 * client.
 */

#pragma once

#include <cstdint>

struct Queue;
struct QueueSelection;
class Response;

void
queue_print_info(Response &r, const Queue &queue,
		 unsigned start, unsigned end);

void
queue_print_uris(Response &r, const Queue &queue,
		 unsigned start, unsigned end);

void
queue_print_changes_info(Response &r, const Queue &queue,
			 uint32_t version,
			 unsigned start, unsigned end);

void
queue_print_changes_position(Response &r, const Queue &queue,
			     uint32_t version,
			     unsigned start, unsigned end);

void
PrintQueue(Response &response, const Queue &queue,
	   const QueueSelection &selection);
