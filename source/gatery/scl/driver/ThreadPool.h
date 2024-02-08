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
#pragma once

/*
 * Do not include the regular gatery stuff since this is meant to compile stand-alone in driver/userspace application code. 
 */

#include <vector>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <functional>
#include <queue>

/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {

	class ThreadPool;

	class TaskGroup {
		public:
			TaskGroup(ThreadPool &pool);
			~TaskGroup();

			void add(std::function<void()> task);
			void flush();
		protected:
			ThreadPool &m_pool;

			size_t m_numTasks = 0;
			size_t m_numTasksDone = 0;
			std::condition_variable m_wakeAwaiter;
			std::mutex m_mutex;

			friend class ThreadPool;
	};

	class ThreadPool
	{
		public:
			ThreadPool(size_t numThreads);
			~ThreadPool();

			void scheduleTask(std::function<void()> task, TaskGroup *group = nullptr);
			inline size_t numWorkers() const { return m_threads.size(); }
		protected:
			std::vector<std::thread> m_threads;
			std::condition_variable m_wakeThreads;
			std::atomic<bool> m_shutdown;
			using Task = std::tuple<std::function<void()>, TaskGroup*>;
			std::queue<Task> m_tasks;
			std::mutex m_mutex;

			void worker();
	};

}