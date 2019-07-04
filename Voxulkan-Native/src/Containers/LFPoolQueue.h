#pragma once
#include <atomic>

template <typename T>
class LFPoolQueue {
public:
	const uint16_t END = 0xffff;

	LFPoolQueue(uint16_t size)
	{
		m_size = size;
		m_pool = new T[size];
		m_ptrs = new std::atomic<uint16_t>[size];
		uint16_t lindex = size - 1;
		for (uint16_t i = 0; i < lindex; i++)
			m_ptrs[i].store(i + 1);
		m_ptrs[lindex].store(END);
		m_next.store(0);
		m_last.store(lindex);
	}
	~LFPoolQueue()
	{
		delete[] m_pool;
		delete[] m_ptrs;
	}

	int dequeue()
	{
		bool wasMaxed = false;
		struct VersionRef
		{
			uint16_t version;
			uint16_t ref;
		};
		uint32_t unsafePtr = m_next.load();
		uint32_t newPtr = 0;
		VersionRef vrOld = {};
		VersionRef vrNew = {};
		do
		{
			memcpy(&vrOld, &unsafePtr, sizeof(uint32_t));

			vrNew.version = (vrOld.version + 1) & 0x7FFFU;

			wasMaxed = vrOld.version & 0x8000U;
			vrNew.ref = m_ptrs[vrOld.ref].load();
			if (vrNew.ref == END)
			{
				if (wasMaxed)
					return END;
				else
				{
					vrNew.ref = vrOld.ref;
					vrNew.version |= 0x8000U;
				}
			}

			memcpy(&newPtr, &vrNew, sizeof(uint32_t));
		} while (!m_next.compare_exchange_weak(unsafePtr, newPtr));

		if (wasMaxed)
			return dequeue();
		else
			return vrOld.ref;
	}

	void enqueue(uint16_t index)
	{
		m_ptrs[index].store(END);
		m_ptrs[m_last.exchange(index)].store(index);
	}
	
	inline T* data() { return m_pool; }

	uint16_t size() { return m_size; }
	T& operator[](const int index) { return m_pool[index]; }

private:
	uint16_t m_size;
	T* m_pool;
	std::atomic<uint16_t>* m_ptrs;

	std::atomic<uint32_t> m_next;
	std::atomic<uint16_t> m_last;
};