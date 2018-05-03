#pragma once
#include "Exception.h"
#include "TypeTraits.h"
#include "XTL.h"
#include "ThreadLocal.h"

class GrandCentralExecuter
{
public:
	typedef std::function<void()> GCETask;

	GrandCentralExecuter(): mRemainTaskCount(0)
	{}

	void DoDispatch(const GCETask& task)
	{
		CRASH_ASSERT(LThreadType == THREAD_IO_WORKER); ///< 일단 IO thread 전용

		
		if (InterlockedIncrement64(&mRemainTaskCount) > 1)
		{
			// 이미 누군가 작업 중이다. concurrent_queue는 스레드 세이프 하므로
			// 큐에다 push 해주고 뜨자.
			mCentralTaskQueue.push(task);
		}
		else
		{
			/// 처음 진입한 놈이 책임지고 다해주자 -.-;

			mCentralTaskQueue.push(task);
			
			while (true)
			{
				GCETask task;
				if (mCentralTaskQueue.try_pop(task))
				{
					task();

					// 큐에 남은 요소가 없다는 거다... 끝나도 된다.
					if (InterlockedDecrement64(&mRemainTaskCount) == 1)
						break;
				}
				else
				{
					// 큐가 비었다면 끝나도된다..
					break;
				}
			}
		}

	}


private:
	typedef concurrency::concurrent_queue<GCETask, STLAllocator<GCETask>> CentralTaskQueue;
	CentralTaskQueue mCentralTaskQueue;
	int64_t mRemainTaskCount;
};

extern GrandCentralExecuter* GGrandCentralExecuter;



template <class T, class F, class... Args>
void GCEDispatch(T instance, F memfunc, Args&&... args)
{
	/// shared_ptr이 아닌 녀석은 받으면 안된다. 작업큐에 들어있는중에 없어질 수 있으니..
	static_assert(true == is_shared_ptr<T>::value, "T should be shared_ptr");

	auto bind = std::bind(&T::memfunc, std::ref(memfunc), std::forward<Args>(args)...);
	GGrandCentralExecuter->DoDispatch(bind);
}