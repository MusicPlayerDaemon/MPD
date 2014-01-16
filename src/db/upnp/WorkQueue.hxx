/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _WORKQUEUE_H_INCLUDED_
#define _WORKQUEUE_H_INCLUDED_

#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"

#include <assert.h>
#include <pthread.h>

#include <string>
#include <queue>

#define LOGINFO(X)
#define LOGERR(X)

/**
 * A WorkQueue manages the synchronisation around a queue of work items,
 * where a number of client threads queue tasks and a number of worker
 * threads take and execute them. The goal is to introduce some level
 * of parallelism between the successive steps of a previously single
 * threaded pipeline. For example data extraction / data preparation / index
 * update, but this could have other uses.
 *
 * There is no individual task status return. In case of fatal error,
 * the client or worker sets an end condition on the queue. A second
 * queue could conceivably be used for returning individual task
 * status.
 */
template <class T>
class WorkQueue {
	// Configuration
	const std::string name;

	// Status
	// Worker threads having called exit
	unsigned n_workers_exited;
	bool ok;

	unsigned n_threads;
	pthread_t *threads;

	// Synchronization
	std::queue<T> queue;
	Cond client_cond;
	Cond worker_cond;
	Mutex mutex;

public:
	/** Create a WorkQueue
	 * @param name for message printing
	 * @param hi number of tasks on queue before clients blocks. Default 0
	 *    meaning no limit. hi == -1 means that the queue is disabled.
	 * @param lo minimum count of tasks before worker starts. Default 1.
	 */
	WorkQueue(const char *_name)
		:name(_name),
		 n_workers_exited(0),
		 ok(false),
		 n_threads(0), threads(nullptr)
	{
	}

	~WorkQueue() {
		setTerminateAndWait();
	}

	/** Start the worker threads.
	 *
	 * @param nworkers number of threads copies to start.
	 * @param start_routine thread function. It should loop
	 *      taking (QueueWorker::take()) and executing tasks.
	 * @param arg initial parameter to thread function.
	 * @return true if ok.
	 */
	bool start(unsigned nworkers, void *(*workproc)(void *), void *arg)
	{
		const ScopeLock protect(mutex);

		assert(nworkers > 0);
		assert(!ok);
		assert(n_threads == 0);
		assert(threads == nullptr);

		n_threads = nworkers;
		threads = new pthread_t[n_threads];

		for  (unsigned i = 0; i < nworkers; i++) {
			int err;
			if ((err = pthread_create(&threads[i], 0, workproc, arg))) {
				LOGERR(("WorkQueue:%s: pthread_create failed, err %d\n",
					name.c_str(), err));
				return false;
			}
		}

		ok = true;
		return true;
	}

	/** Add item to work queue, called from client.
	 *
	 * Sleeps if there are already too many.
	 */
	template<typename U>
	bool put(U &&u)
	{
		const ScopeLock protect(mutex);

		queue.emplace(std::forward<U>(u));

		// Just wake one worker, there is only one new task.
		worker_cond.signal();

		return true;
	}


	/** Tell the workers to exit, and wait for them.
	 */
	void setTerminateAndWait()
	{
		const ScopeLock protect(mutex);

		// Wait for all worker threads to have called workerExit()
		ok = false;
		while (n_workers_exited < n_threads) {
			worker_cond.broadcast();
			client_cond.wait(mutex);
		}

		// Perform the thread joins and compute overall status
		// Workers return (void*)1 if ok
		for (unsigned i = 0; i < n_threads; ++i) {
			void *status;
			pthread_join(threads[i], &status);
		}

		delete[] threads;
		threads = nullptr;
		n_threads = 0;

		// Reset to start state.
		n_workers_exited = 0;
	}

	/** Take task from queue. Called from worker.
	 *
	 * Sleeps if there are not enough. Signal if we go to sleep on empty
	 * queue: client may be waiting for our going idle.
	 */
	bool take(T &tp)
	{
		const ScopeLock protect(mutex);

		if (!ok)
			return false;

		while (queue.empty()) {
			worker_cond.wait(mutex);
			if (!ok)
				return false;
		}

		tp = std::move(queue.front());
		queue.pop();
		return true;
	}

	/** Advertise exit and abort queue. Called from worker
	 *
	 * This would happen after an unrecoverable error, or when
	 * the queue is terminated by the client. Workers never exit normally,
	 * except when the queue is shut down (at which point ok is set to
	 * false by the shutdown code anyway). The thread must return/exit
	 * immediately after calling this.
	 */
	void workerExit()
	{
		const ScopeLock protect(mutex);

		n_workers_exited++;
		ok = false;
		client_cond.broadcast();
	}
};

#endif /* _WORKQUEUE_H_INCLUDED_ */
