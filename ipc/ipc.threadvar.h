#ifndef __IPC_THREADVAR__
#define __IPC_THREADVAR__

#include <memory>
#include <thread>
#include <mutex>
#include <unordered_map>

#include "ipc.noncopyable.h"

namespace ipc
{
	template <class T>
	class threadvar : public noncopyable
	{
		std::mutex mutex_;
		std::unordered_map<std::thread::id, std::shared_ptr<T>> threads_;
	public:
		std::shared_ptr<T> get(void);
	};

	template <class T>
	std::shared_ptr<T> threadvar<T>::get(void)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		typename std::unordered_map<std::thread::id, std::shared_ptr<T>>
			::iterator itr = threads_.find(std::this_thread::get_id());
		if (itr != threads_.end())
			return itr->second;
		return threads_.insert(
			std::make_pair(std::this_thread::get_id(), std::make_shared<T>())
		).first->second;
	}
}

#endif



