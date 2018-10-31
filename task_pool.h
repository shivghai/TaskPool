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
	std::vector<std::thread> d_threads;
	std::condition_variable d_cv;
	bool d_terminate;

	void startProcessing();

	public:

	TaskPool()
	{
		d_terminate = false;
		d_threads.resize(std::thread::hardware_concurrency());
		for (auto & t : d_threads)
			t = std::thread([this]() {
					startProcessing();
					});
	}

	void push(F& func, T& args);

	std::tuple<F*, T*> pop();

	void joinAll();
};

template <typename F, typename T>
inline
void TaskPool<F, T>::joinAll()
{
	d_terminate = true;
	d_cv.notify_all();
	for (auto & t : d_threads)
		t.join();
}

template <typename F, typename T>
inline
void TaskPool<F, T>::push(F& func, T& args)
{
	std::lock_guard<std::mutex> guard(d_mutex);
	d_taskQueue.push({&func, &args});
	d_cv.notify_one();
}

template <typename F, typename T>
inline
void TaskPool<F, T>::startProcessing()
{
	while (!d_terminate || !d_taskQueue.empty())
	{
		std::unique_lock<std::mutex> lock(d_mutex);

		d_cv.wait(lock, [this]() {
				return !d_taskQueue.empty() || d_terminate;
				});

		if (d_terminate && d_taskQueue.empty())
			return;
		
		auto x = pop();

		auto* func = std::get<0>(x);
		auto* args = std::get<1>(x);

		lock.unlock();
		(*func)((*args));
		d_cv.notify_one();
	}
}

template <typename F, typename T>
inline
std::tuple<F*, T*> TaskPool<F, T>::pop()
{
	auto x = std::move(d_taskQueue.front());
	d_taskQueue.pop();
	return x;
}


}

