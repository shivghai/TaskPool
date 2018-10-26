#include <iostream>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <thread>

namespace pool {

template <typename F, typename T>
class TaskPool {
	std::mutex d_mutex;
	std::queue<std::tuple<F, T>> d_taskQueue;
	std::vector<std::thread> threads;
	std::condition_variable d_cv;
	bool terminate;

	void start_processing()
	{
		while (!terminate || !d_taskQueue.empty())
		{
			std::unique_lock<std::mutex> lock(d_mutex);

			d_cv.wait(lock, [this]() { return !d_taskQueue.empty() || terminate; });
		// now the given thread has the lock
			if (terminate && d_taskQueue.empty())
				return;
			
			// get the tuple
			auto x = pop();
			auto func = std::get<0>(x);
			auto args = std::get<1>(x);
			lock.unlock();
			func(args);
			d_cv.notify_one();
		}
	}

	public:

	int count;
	TaskPool()
	{
		count = 0;
		terminate = false;
		threads.resize(std::thread::hardware_concurrency());
		for (auto & t : threads)
			t = std::thread([this]() { TaskPool::start_processing(); });
	}

	void push(F func, T args)
	{
		std::lock_guard<std::mutex> guard(d_mutex);
		d_taskQueue.push({func, args});
		d_cv.notify_one();
	}

	std::tuple<F, T> pop()
	{
		auto x = d_taskQueue.front();
		d_taskQueue.pop();
		return x;
	}

	void join_all()
	{
		terminate = true;
		d_cv.notify_all();
		for (auto & t : threads)
			t.join();
	}
};

}

