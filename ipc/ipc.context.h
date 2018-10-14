#ifndef __IPC_CONTEXT__
#define __IPC_CONTEXT__

#include <memory>
#include <vector>
#include <mutex>
#include <utility>
#include <condition_variable>

#include "ipc.threadvar.h"
#include "ipc.noncopyable.h"

namespace ipc
{
	struct channable;

	class context : public noncopyable,
			public std::enable_shared_from_this<context>
	{
		static threadvar<context> context_;
		std::condition_variable cond_;
		unsigned long count_;

		int unblockedx_;
		void* recv_data_;

		std::vector<std::pair<channable*, void*>> send_data_;
	public:
		static std::mutex mutex;
	public:
		context(void);
		virtual ~context(void);
	public:
		static std::shared_ptr<context> get(void);
	public:
		void add(channable* chan, void* data);
		void add(channable* chan);
	public:
		void add_to_all_channels(void);
		void remove_from_all_channels(void);
	public:
		void clear(void);
	public:
		int get_unblocked_index(void) const;
	public:
		void* get_receive_data(void) const;
	public:
		void* unblocked_sender(channable* chan);
		void unblocked_receiver(channable* chan, void* data);
	public:
		void signal(void);
		void wait(void);
	public:
		std::size_t send_data_size(void) const;
		channable* send_data_channel(const int& i) const;
		void* send_data_data(const int& i) const;
	};
}

#endif

