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
#include <unordered_map>

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
	/**
	 * Store per-worker-thread data. Just an initialized timespec,
	 * and used at the moment.
	 */
	class WQTData {
	public:
		WQTData() {wstart.tv_sec = 0; wstart.tv_nsec = 0;}
		struct timespec wstart;
	};

	// Configuration
	std::string m_name;
	size_t m_high;
	size_t m_low;

	// Status
	// Worker threads having called exit
	unsigned int m_workers_exited;
	bool m_ok;

	// Per-thread data. The data is not used currently, this could be
	// a set<pthread_t>
	std::unordered_map<pthread_t, WQTData> m_worker_threads;

	// Synchronization
	std::queue<T> m_queue;
	Cond m_ccond;
	Cond m_wcond;
	Mutex m_mutex;
	// Client/Worker threads currently waiting for a job
	unsigned int m_clients_waiting;
	unsigned int m_workers_waiting;

public:
	/** Create a WorkQueue
	 * @param name for message printing
	 * @param hi number of tasks on queue before clients blocks. Default 0
	 *    meaning no limit. hi == -1 means that the queue is disabled.
	 * @param lo minimum count of tasks before worker starts. Default 1.
	 */
	WorkQueue(const char *name, size_t hi = 0, size_t lo = 1)
		:m_name(name), m_high(hi), m_low(lo),
		 m_workers_exited(0),
		 m_ok(true),
		 m_clients_waiting(0), m_workers_waiting(0)
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
		const ScopeLock protect(m_mutex);

		for  (int i = 0; i < nworkers; i++) {
			int err;
			pthread_t thr;
			if ((err = pthread_create(&thr, 0, workproc, arg))) {
				LOGERR(("WorkQueue:%s: pthread_create failed, err %d\n",
					m_name.c_str(), err));
				return false;
			}
			m_worker_threads.insert(std::make_pair(thr, WQTData()));
		}
		return true;
	}

	/** Add item to work queue, called from client.
	 *
	 * Sleeps if there are already too many.
	 */
	bool put(T t)
	{
		const ScopeLock protect(m_mutex);

		if (!ok()) {
			LOGERR(("WorkQueue::put:%s: !ok or mutex_lock failed\n",
				m_name.c_str()));
			return false;
		}

		while (ok() && m_high > 0 && m_queue.size() >= m_high) {
			// Keep the order: we test ok() AFTER the sleep...
			m_clients_waiting++;
			m_ccond.wait(m_mutex);
			if (!ok()) {
				m_clients_waiting--;
				return false;
			}
			m_clients_waiting--;
		}

		m_queue.push(t);
		if (m_workers_waiting > 0) {
			// Just wake one worker, there is only one new task.
			m_wcond.signal();
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
		const ScopeLock protect(m_mutex);

		if (!ok()) {
			LOGERR(("WorkQueue::waitIdle:%s: not ok or can't lock\n",
				m_name.c_str()));
			return false;
		}

		// We're done when the queue is empty AND all workers are back
		// waiting for a task.
		while (ok() && (m_queue.size() > 0 ||
				m_workers_waiting != m_worker_threads.size())) {
			m_clients_waiting++;
			m_ccond.wait(m_mutex);
			m_clients_waiting--;
		}

		return ok();
	}


	/** Tell the workers to exit, and wait for them.
	 *
	 * Does not bother about tasks possibly remaining on the queue, so
	 * should be called after waitIdle() for an orderly shutdown.
	 */
	void setTerminateAndWait()
	{
		const ScopeLock protect(m_mutex);

		if (m_worker_threads.empty())
			// Already called ?
			return;

		// Wait for all worker threads to have called workerExit()
		m_ok = false;
		while (m_workers_exited < m_worker_threads.size()) {
			m_wcond.broadcast();
			m_clients_waiting++;
			m_ccond.wait(m_mutex);
			m_clients_waiting--;
		}

		// Perform the thread joins and compute overall status
		// Workers return (void*)1 if ok
		while (!m_worker_threads.empty()) {
			void *status;
			auto it = m_worker_threads.begin();
			pthread_join(it->first, &status);
			m_worker_threads.erase(it);
		}

		// Reset to start state.
		m_workers_exited = m_clients_waiting = m_workers_waiting = 0;
		m_ok = true;
	}

	/** Take task from queue. Called from worker.
	 *
	 * Sleeps if there are not enough. Signal if we go to sleep on empty
	 * queue: client may be waiting for our going idle.
	 */
	bool take(T &tp)
	{
		const ScopeLock protect(m_mutex);

		if (!ok()) {
			return false;
		}

		while (ok() && m_queue.size() < m_low) {
			m_workers_waiting++;
			if (m_queue.empty())
				m_ccond.broadcast();
			m_wcond.wait(m_mutex);
			if (!ok()) {
				// !ok is a normal condition when shutting down
				if (ok()) {
					LOGERR(("WorkQueue::take:%s: cond_wait failed or !ok\n",
						m_name.c_str()));
				}
				m_workers_waiting--;
				return false;
			}
			m_workers_waiting--;
		}

		tp = m_queue.front();
		m_queue.pop();
		if (m_clients_waiting > 0) {
			// No reason to wake up more than one client thread
			m_ccond.signal();
		}
		return true;
	}

	/** Advertise exit and abort queue. Called from worker
	 *
	 * This would happen after an unrecoverable error, or when
	 * the queue is terminated by the client. Workers never exit normally,
	 * except when the queue is shut down (at which point m_ok is set to
	 * false by the shutdown code anyway). The thread must return/exit
	 * immediately after calling this.
	 */
	void workerExit()
	{
		const ScopeLock protect(m_mutex);

		m_workers_exited++;
		m_ok = false;
		m_ccond.broadcast();
	}

private:
	bool ok()
	{
		return m_ok && m_workers_exited == 0 && !m_worker_threads.empty();
	}
};

#endif /* _WORKQUEUE_H_INCLUDED_ */
