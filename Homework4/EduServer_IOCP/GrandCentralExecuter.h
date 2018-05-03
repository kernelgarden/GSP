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
		CRASH_ASSERT(LThreadType == THREAD_IO_WORKER); ///< �ϴ� IO thread ����

		
		if (InterlockedIncrement64(&mRemainTaskCount) > 1)
		{
			// �̹� ������ �۾� ���̴�. concurrent_queue�� ������ ������ �ϹǷ�
			// ť���� push ���ְ� ����.
			mCentralTaskQueue.push(task);
		}
		else
		{
			/// ó�� ������ ���� å������ �������� -.-;

			mCentralTaskQueue.push(task);
			
			while (true)
			{
				GCETask task;
				if (mCentralTaskQueue.try_pop(task))
				{
					task();

					// ť�� ���� ��Ұ� ���ٴ� �Ŵ�... ������ �ȴ�.
					if (InterlockedDecrement64(&mRemainTaskCount) == 1)
						break;
				}
				else
				{
					// ť�� ����ٸ� �������ȴ�..
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
	/// shared_ptr�� �ƴ� �༮�� ������ �ȵȴ�. �۾�ť�� ����ִ��߿� ������ �� ������..
	static_assert(true == is_shared_ptr<T>::value, "T should be shared_ptr");

	auto bind = std::bind(&T::memfunc, std::ref(memfunc), std::forward<Args>(args)...);
	GGrandCentralExecuter->DoDispatch(bind);
}