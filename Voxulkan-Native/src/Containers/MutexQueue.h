#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

template <typename T>
class MutexQueue
{
public:
	MutexQueue();
	~MutexQueue();

	T& front();
	void pop_front();
	void pop_front(T& item);

	void process_front(std::function<void(const T& item)>& process);

	void push_back(const T& item);
	void push_back(T&& item);

	size_t size();
	bool empty();

private:
	std::deque<T> queue_;
	std::mutex mutex_;
	std::condition_variable cond_;
};


template <typename T>
MutexQueue<T>::MutexQueue() {}

template <typename T>
MutexQueue<T>::~MutexQueue() {}

template <typename T>
T& MutexQueue<T>::front()
{
	std::unique_lock<std::mutex> mlock(mutex_);
	while (queue_.empty())
	{
		cond_.wait(mlock);
	}
	return queue_.front();
}

template <typename T>
void MutexQueue<T>::pop_front()
{
	std::unique_lock<std::mutex> mlock(mutex_);
	while (queue_.empty())
	{
		cond_.wait(mlock);
	}
	queue_.pop_front();
}

template<typename T>
void MutexQueue<T>::pop_front(T& item)
{
	std::unique_lock<std::mutex> mlock(mutex_);
	while (queue_.empty())
	{
		cond_.wait(mlock);
	}
	item = queue_.front();
	queue_.pop_front();
}

template<typename T>
inline void MutexQueue<T>::process_front(std::function<void(const T& item)>& process)
{
	std::unique_lock<std::mutex> mlock(mutex_);
	while (queue_.empty())
	{
		cond_.wait(mlock);
	}
	T& item = queue_.front();
	queue_.pop_front();
	process(item);
	queue_.push_back(item);
}

template <typename T>
void MutexQueue<T>::push_back(const T& item)
{
	std::unique_lock<std::mutex> mlock(mutex_);
	queue_.push_back(item);
	mlock.unlock();     // unlock before notificiation to minimize mutex con
	cond_.notify_one(); // notify one waiting thread

}

template <typename T>
void MutexQueue<T>::push_back(T&& item)
{
	std::unique_lock<std::mutex> mlock(mutex_);
	queue_.push_back(std::move(item));
	mlock.unlock();     // unlock before notificiation to minimize mutex con
	cond_.notify_one(); // notify one waiting thread

}

template <typename T>
size_t MutexQueue<T>::size()
{
	std::unique_lock<std::mutex> mlock(mutex_);
	size_t size = queue_.size();
	mlock.unlock();
	return size;
}

template <typename T>
bool MutexQueue<T>::empty()
{
	std::unique_lock<std::mutex> mlock(mutex_);
	bool e = queue_.empty();
	mlock.unlock();
	return e;
}