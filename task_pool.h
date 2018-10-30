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
	std::queue<std::tuple<F*, T*>> d_taskQueue;
	std::vector<std::thread> threads;
	std::condition_variable d_cv;
	bool terminate;

	void startProcessing()
	{
		while (!terminate || !d_taskQueue.empty())
		{
			std::unique_lock<std::mutex> lock(d_mutex);

			d_cv.wait(lock, [this]() {
					return !d_taskQueue.empty() || terminate;
					});

			if (terminate && d_taskQueue.empty())
				return;
			
			auto x = pop();

			auto* func = std::get<0>(x);
			auto* args = std::get<1>(x);

			lock.unlock();
			(*func)((*args));
			d_cv.notify_one();
		}
	}

	public:

	TaskPool()
	{
		terminate = false;
		threads.resize(std::thread::hardware_concurrency());
		for (auto & t : threads)
			t = std::thread([this]() {
					startProcessing();
					});
	}

	void push(F& func, T& args)
	{
		std::lock_guard<std::mutex> guard(d_mutex);
		d_taskQueue.push({&func, &args});
		d_cv.notify_one();
	}

	std::tuple<F*, T*> pop()
	{
		auto x = std::move(d_taskQueue.front());
		d_taskQueue.pop();
		return x;
	}

	void joinAll()
	{
		terminate = true;
		d_cv.notify_all();
		for (auto & t : threads)
			t.join();
	}
};

}

