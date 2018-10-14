#ifndef __IPC_SELECTOR__
#define __IPC_SELECTOR__

#include "ipc.channel.h"
#include "ipc.noncopyable.h"

#include <memory>

namespace ipc
{
	class selector : public noncopyable
	{
		void* data_;
		std::vector<std::pair<channable*, void*>> send_data_;
	public:
		selector(void);
		virtual ~selector(void);
	public:
		template <class T>
		void send(channel<T>& chan, const T& data);
		template <class T>
		void send(const std::shared_ptr<channel<T>>& chan, const T& data);
	public:
		template <class T>
		void recv(channel<T>& chan);
		template <class T>
		void recv(const std::shared_ptr<channel<T>>& chan);
	public:
		template <class T>
		T get_data(void) const;
	public:
		void clear(void);
	public:
		int select(const bool& block = true);
	private:
		void set_data(void* data);
	};

	template <class T>
	void selector::send(channel<T>& chan, const T& data)
	{
		send_data_.push_back(std::make_pair(&chan, new T(data)));
	}

	template <class T>
	void selector::send(const std::shared_ptr<channel<T>>& chan, const T& data)
	{
		send_data_.push_back(std::make_pair(chan.get(), new T(data)));
	}

	template <class T>
	void selector::recv(channel<T>& chan)
	{
		send_data_.push_back(std::make_pair(&chan, nullptr));
	}

	template <class T>
	void selector::recv(const std::shared_ptr<channel<T>>& chan)
	{
		send_data_.push_back(std::make_pair(chan.get(), nullptr));
	}

	template <class T>
	T selector::get_data(void) const
	{
		return *static_cast<T*>(data_);
	}
};

#endif
