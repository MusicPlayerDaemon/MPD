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

#include <pthread.h>
#include <time.h>

#include <string>
#include <queue>
#include <list>

//#include "debuglog.h"
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
	const size_t high;
	const size_t low;

	// Status
	// Worker threads having called exit
	unsigned n_workers_exited;
	bool ok;

	std::list<pthread_t> threads;

	// Synchronization
	std::queue<T> queue;
	Cond client_cond;
	Cond worker_cond;
	Mutex mutex;
	// Client/Worker threads currently waiting for a job
	unsigned n_clients_waiting;
	unsigned n_workers_waiting;

public:
	/** Create a WorkQueue
	 * @param name for message printing
	 * @param hi number of tasks on queue before clients blocks. Default 0
	 *    meaning no limit. hi == -1 means that the queue is disabled.
	 * @param lo minimum count of tasks before worker starts. Default 1.
	 */
	WorkQueue(const char *_name, size_t hi = 0, size_t lo = 1)
		:name(_name), high(hi), low(lo),
		 n_workers_exited(0),
		 ok(true),
		 n_clients_waiting(0), n_workers_waiting(0)
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
	bool start(int nworkers, void *(*workproc)(void *), void *arg)
	{
		const ScopeLock protect(mutex);

		for  (int i = 0; i < nworkers; i++) {
			int err;
			pthread_t thr;
			if ((err = pthread_create(&thr, 0, workproc, arg))) {
				LOGERR(("WorkQueue:%s: pthread_create failed, err %d\n",
					name.c_str(), err));
				return false;
			}
			threads.push_back(thr);
		}
		return true;
	}

	/** Add item to work queue, called from client.
	 *
	 * Sleeps if there are already too many.
	 */
	bool put(T t)
	{
		const ScopeLock protect(mutex);

		if (!IsOK()) {
			LOGERR(("WorkQueue::put:%s: !ok or mutex_lock failed\n",
				name.c_str()));
			return false;
		}

		while (IsOK() && high > 0 && queue.size() >= high) {
			// Keep the order: we test IsOK() AFTER the sleep...
			n_clients_waiting++;
			client_cond.wait(mutex);
			if (!IsOK()) {
				n_clients_waiting--;
				return false;
			}
			n_clients_waiting--;
		}

		queue.push(t);
		if (n_workers_waiting > 0) {
			// Just wake one worker, there is only one new task.
			worker_cond.signal();
		}

		return true;
	}

	/**
	 * Wait until the queue is inactive. Called from client.
	 *
	 * Waits until the task queue is empty and the workers are all
	 * back sleeping. Used by the client to wait for all current work
	 * to be completed, when it needs to perform work that couldn't be
	 * done in parallel with the worker's tasks, or before shutting
	 * down. Work can be resumed after calling this. Note that the
	 * only thread which can call it safely is the client just above
	 * (which can control the task flow), else there could be
	 * tasks in the intermediate queues.
	 * To rephrase: there is no warranty on return that the queue is actually
	 * idle EXCEPT if the caller knows that no jobs are still being created.
	 * It would be possible to transform this into a safe call if some kind
	 * of suspend condition was set on the queue by waitIdle(), to be reset by
	 * some kind of "resume" call. Not currently the case.
	 */
	bool waitIdle()
	{
		const ScopeLock protect(mutex);

		if (!IsOK()) {
			LOGERR(("WorkQueue::waitIdle:%s: not ok or can't lock\n",
				name.c_str()));
			return false;
		}

		// We're done when the queue is empty AND all workers are back
		// waiting for a task.
		while (IsOK() && (queue.size() > 0 ||
				n_workers_waiting != threads.size())) {
			n_clients_waiting++;
			client_cond.wait(mutex);
			n_clients_waiting--;
		}

		return IsOK();
	}


	/** Tell the workers to exit, and wait for them.
	 *
	 * Does not bother about tasks possibly remaining on the queue, so
	 * should be called after waitIdle() for an orderly shutdown.
	 */
	void setTerminateAndWait()
	{
		const ScopeLock protect(mutex);

		if (threads.empty())
			// Already called ?
			return;

		// Wait for all worker threads to have called workerExit()
		ok = false;
		while (n_workers_exited < threads.size()) {
			worker_cond.broadcast();
			n_clients_waiting++;
			client_cond.wait(mutex);
			n_clients_waiting--;
		}

		// Perform the thread joins and compute overall status
		// Workers return (void*)1 if ok
		while (!threads.empty()) {
			void *status;
			auto thread = threads.front();
			pthread_join(thread, &status);
			threads.pop_front();
		}

		// Reset to start state.
		n_workers_exited = n_clients_waiting = n_workers_waiting = 0;
		ok = true;
	}

	/** Take task from queue. Called from worker.
	 *
	 * Sleeps if there are not enough. Signal if we go to sleep on empty
	 * queue: client may be waiting for our going idle.
	 */
	bool take(T &tp)
	{
		const ScopeLock protect(mutex);

		if (!IsOK()) {
			return false;
		}

		while (IsOK() && queue.size() < low) {
			n_workers_waiting++;
			if (queue.empty())
				client_cond.broadcast();
			worker_cond.wait(mutex);
			if (!IsOK()) {
				// !ok is a normal condition when shutting down
				if (IsOK()) {
					LOGERR(("WorkQueue::take:%s: cond_wait failed or !ok\n",
						name.c_str()));
				}
				n_workers_waiting--;
				return false;
			}
			n_workers_waiting--;
		}

		tp = queue.front();
		queue.pop();
		if (n_clients_waiting > 0) {
			// No reason to wake up more than one client thread
			client_cond.signal();
		}
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

private:
	bool IsOK()
	{
		return ok && n_workers_exited == 0 && !threads.empty();
	}
};

#endif /* _WORKQUEUE_H_INCLUDED_ */
