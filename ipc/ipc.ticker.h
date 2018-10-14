#ifndef __IPC_TICKER__
#define __IPC_TICKER__

#include <cstdint>
#include <thread>

#include "ipc.channel.h"
#include "ipc.scheduler.h"
#include "ipc.noncopyable.h" 

namespace ipc
{
	class ticker : public noncopyable
	{
		scheduler timer_;
		std::thread runner_;
	public:
		channel<bool> c;
	public:
		ticker(const std::chrono::system_clock::duration& d);
		virtual ~ticker(void);
	public:
		void stop(void);
	private:
		void send_time(void);
	};
}

#endif
