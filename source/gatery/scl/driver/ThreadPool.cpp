/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

	Gatery is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	Gatery is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 * Do not include the regular gatery stuff since this is meant to compile stand-alone in driver/userspace application code. 
 */

#include "ThreadPool.h"

/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {

	TaskGroup::TaskGroup(ThreadPool &pool) : m_pool(pool)
	{

	}

	TaskGroup::~TaskGroup()
	{
		flush();
	}

	void TaskGroup::add(std::function<void()> task)
	{
		m_numTasks++;
		m_pool.scheduleTask(std::move(task), this);
	}

	void TaskGroup::flush()
	{
		std::unique_lock lock(m_mutex);
		while (m_numTasksDone != m_numTasks)
			m_wakeAwaiter.wait(lock);
	}

	ThreadPool::ThreadPool(size_t numThreads)
	{
		m_shutdown.store(false);

		m_threads.resize(numThreads);
		for (auto &t : m_threads)
			t = std::thread(std::bind(&ThreadPool::worker, this));
	}

	ThreadPool::~ThreadPool()
	{
		{
			std::unique_lock lock(m_mutex);
			m_shutdown.store(true);
			m_wakeThreads.notify_all();
		}
		for (auto &t : m_threads)
			t.join();
	}

	void ThreadPool::scheduleTask(std::function<void()> task, TaskGroup *group)
	{
		std::unique_lock lock(m_mutex);
		m_tasks.push({std::move(task), group});
		m_wakeThreads.notify_one();
	}

	void ThreadPool::worker()
	{
		while (!m_shutdown.load()) {
			Task task;
			{
				std::unique_lock lock(m_mutex);
				while (m_tasks.empty()) {
					if (m_shutdown.load()) return;
					m_wakeThreads.wait(lock);
				}
				task = std::move(m_tasks.front());
				m_tasks.pop();
			}
			std::get<0>(task)();

			TaskGroup *group = std::get<1>(task);
			if (group != nullptr) {
				std::unique_lock lock(group->m_mutex);
				if (++group->m_numTasksDone == group->m_numTasks)
					group->m_wakeAwaiter.notify_one();
			}
		}
	}


}