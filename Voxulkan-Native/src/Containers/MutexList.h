#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>

template <typename T>
class MutexList {
public:
	MutexList() : vec(), mut(), cond() {}
	MutexList(const MutexList<T>& orig) : vec(orig.vec), mut(), cond() {}
	MutexList(MutexList<T>&& orig)
		: vec{ std::move(orig.vec) },
		mut{},
		cond{}
	{}
	MutexList(std::vector<T> vec)
		: vec{ std::move(vec) },
		mut{},
		cond{}
	{}

	void erase()
	{
		std::lock_guard<std::mutex> lock(mut);
		vec.clear();
		cond.notify_one();
	}
	void swap(std::vector<T>& rep)
	{
		std::lock_guard<std::mutex> lock(mut);
		vec.swap(rep);
		cond.notify_one();
	}
	void set(const T& in, const int index)
	{
		std::lock_guard<std::mutex> lock(mut);
		vec[index] = std::move(in);
		cond.notify_one();
	}
	void add(const std::vector<T>& v)
	{
		std::lock_guard<std::mutex> lock(mut);
		vec.insert(vec.end(), v.begin(), v.end());
		cond.notify_one();
	}
	void add(const T &in)
	{
		std::lock_guard<std::mutex> lock(mut);
		vec.push_back(std::move(in));
		cond.notify_one();
	}
	T& operator[](const int index)
	{
		return vec[index];
	}

private:
	std::vector<T> vec;
	std::mutex mut = {};
	std::condition_variable cond = {};
};