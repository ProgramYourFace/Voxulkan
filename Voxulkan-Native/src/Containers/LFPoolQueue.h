#pragma once
#include <atomic>

#ifndef  POOL_END
#define POOL_END 0xffff
#endif

template <typename T>
class LFPoolQueue {
public:

	LFPoolQueue(uint16_t size)
	{
		m_size = size;
		m_pool = new T[size];
		m_ptrs = new std::atomic<uint16_t>[size];
		uint16_t lindex = size - 1;
		for (uint16_t i = 0; i < lindex; i++)
			m_ptrs[i].store(i + 1);
		m_ptrs[lindex].store(POOL_END);
		m_next.store(0);
		m_last.store(lindex);
	}
	~LFPoolQueue()
	{
		delete[] m_pool;
		delete[] m_ptrs;
	}

	struct VersionRef
	{
		uint16_t version;
		uint16_t ref;
	};

	uint16_t dequeue()
	{
		uint32_t unsafePtr = m_next.load();
		uint32_t newPtr = 0;
		VersionRef vrOld = {};
		VersionRef vrNew = {};
		do
		{
			memcpy(&vrOld, &unsafePtr, sizeof(uint32_t));

			if (vrOld.ref == POOL_END)
				return POOL_END;

			vrNew.version = vrOld.version + 1;
			vrNew.ref = m_ptrs[vrOld.ref].load();

			if (vrNew.ref == POOL_END)
			{
				m_key.store(vrOld.ref, std::memory_order_release);
			}

			memcpy(&newPtr, &vrNew, sizeof(uint32_t));
		} while (!m_next.compare_exchange_weak(unsafePtr, newPtr));

		return vrOld.ref;
	}

	void enqueue(uint16_t index)
	{
		m_ptrs[index].store(POOL_END);
		uint16_t lock = m_last.exchange(index);
		m_ptrs[lock].store(index);

		uint16_t k = m_key.load(std::memory_order_acquire);
		if (k == lock)
		{
			uint32_t oldPtr = m_next.load();
			VersionRef vrOld = {};
		
			memcpy(&vrOld, &oldPtr, sizeof(uint32_t));
			if (vrOld.ref == POOL_END)
			{
				VersionRef vrNew = {};

				vrNew.ref = index;
				vrNew.version = vrOld.version + 1;

				uint32_t newPtr = 0;
				memcpy(&newPtr, &vrNew, sizeof(uint32_t));

				m_next.store(newPtr);
			}
		}
	}
	
	inline T* data() { return m_pool; }

	uint16_t size() { return m_size; }
	T& operator[](const uint16_t index) { return m_pool[index]; }

private:
	uint16_t m_size;
	T* m_pool;
	std::atomic<uint16_t>* m_ptrs;

	std::atomic<uint32_t> m_next;
	std::atomic<uint16_t> m_key;
	std::atomic<uint16_t> m_last;
};