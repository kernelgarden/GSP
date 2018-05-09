#pragma once

#include "TypeTraits.h"
#include "FastSpinlock.h"
#include "Timer.h"


class SyncExecutable : public std::enable_shared_from_this<SyncExecutable>
{
public:
	SyncExecutable() : mLock(LO_BUSINESS_CLASS)
	{}
	virtual ~SyncExecutable() {}

	template <class R, class T, class... Args>
	R DoSync(R (T::*memfunc)(Args...), Args... args)
	{
		static_assert(true == std::is_convertible<T*, SyncExecutable*>::value, "T should be derived from SyncExecutable");

		FastSpinlockGuard criticalSection(mLock);
		auto func = std::bind(memfunc, GetSharedFromThis<T>(), std::forward<Args>(args)...);
		return static_cast<R>(func());
		//return (GetSharedFromThis<T>()->memfunc)(args...);
		/*
		EnterLock();
		auto aaa = GetSharedFromThis<T>();
		//(std::static_pointer_cast<T>(this)->*memfunc)(args...);
		//std::bind(memfunc, this, args...)();
		//(*this.*memfunc)(args...);
		//auto ret = (*(T*)this).*memfunc(std::forward<Args>(args)...);
		LeaveLock();
		*/
	}
	

	void EnterLock() { mLock.EnterWriteLock(); }
	void LeaveLock() { mLock.LeaveWriteLock(); }
	
 	template <class T>
	std::shared_ptr<T> GetSharedFromThis()
 	{
		static_assert(true == std::is_convertible<T*, SyncExecutable*>::value, "T should be derived from SyncExecutable");
 		
		//return std::shared_ptr<T>((Player*)this); ///< 이렇게 하면 안될걸???
		return std::static_pointer_cast<T>(shared_from_this());
 	}

private:

	FastSpinlock mLock;
};


template <class T, class F, class... Args>
void DoSyncAfter(uint32_t after, T instance, F memfunc, Args&&... args)
{
	static_assert(true == is_shared_ptr<T>::value, "T should be shared_ptr");
	static_assert(true == std::is_convertible<T, std::shared_ptr<SyncExecutable>>::value, "T should be shared_ptr SyncExecutable");

	LTimer->PushTimerJob(instance, std::bind(memfunc, instance, std::forward<Args>(args)...), after);
}