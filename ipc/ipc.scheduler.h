#ifndef __IPC_SCHEDULER__
#define __IPC_SCHEDULER__

#include <functional>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <map>

#include "ipc.noncopyable.h" 

namespace ipc
{
	typedef std::function<void(void)> func;

	class scheduler : public noncopyable
	{
		std::multimap<std::chrono::system_clock::time_point, func> tasks_;
		std::condition_variable cond_;
		std::mutex mutex_;
		int nthreads_;
		bool stop_requested_;
		bool stop_when_empty_;
	public:
		scheduler(void);
		virtual ~scheduler(void);
	public:
		void run(void);
	public:
		void stop(const bool& drain = false);
	public:
		void schedule(const func& f,
			const std::chrono::system_clock::time_point& t);
		void schedule(const func& f,
			const std::chrono::system_clock::duration& s);
		void schedule(const func& f,
			const std::chrono::system_clock::duration& s,
			const std::chrono::system_clock::duration& d);
	};
}

#endif
