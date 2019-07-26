#pragma once
#include <atomic>
#include <string>
#include <sstream>

#ifdef  END
#undef END
#endif
#define POOL_QUEUE_END 0xffff

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
		m_ptrs[lindex].store(POOL_QUEUE_END);
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

			if (vrOld.ref == POOL_QUEUE_END)
				return POOL_QUEUE_END;

			vrNew.version = vrOld.version + 1;
			vrNew.ref = m_ptrs[vrOld.ref].load();

			memcpy(&newPtr, &vrNew, sizeof(uint32_t));
		} while (!m_next.compare_exchange_weak(unsafePtr, newPtr));

		return vrOld.ref;
	}

	void enqueue(uint16_t index)
	{
		m_ptrs[index].store(POOL_QUEUE_END);

		uint32_t unsafePtr = m_next.load();
		uint32_t newPtr = 0;
		VersionRef vrOld = {};
		VersionRef vrNew = {};
		memcpy(&vrOld, &unsafePtr, sizeof(uint32_t));
		vrOld.ref = POOL_QUEUE_END;
		memcpy(&unsafePtr, &vrOld, sizeof(uint32_t));
		vrNew.ref = index;
		vrNew.version = vrOld.version + 1;
		memcpy(&newPtr, &vrNew, sizeof(uint32_t));
		m_next.compare_exchange_strong(unsafePtr, newPtr);

		m_ptrs[m_last.exchange(index)].store(index);

	}
	
	std::string getPtrsString()
	{
		std::stringstream ss;
		ss << "[";
		for (int i = 0; i < m_size;i++)
		{
			ss << m_ptrs[i].load();
			if (i != m_size - 1)
			{
				ss << ",";
			}
		}
		ss << "] \n";
		struct VersionRef
		{
			uint16_t version;
			uint16_t ref;
		};
		VersionRef vrOld = {};
		uint32_t t = m_next.load();
		memcpy(&vrOld, &t, sizeof(uint32_t));
		ss << vrOld.ref << "-"<< m_last.load();
		return ss.str();
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