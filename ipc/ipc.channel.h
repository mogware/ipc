#ifndef __IPC_CHANNEL__
#define __IPC_CHANNEL__

#include <atomic>
#include <memory>
#include <vector>
#include <array>
#include <mutex>
#include <stdexcept>

#include "ipc.context.h"
#include "ipc.noncopyable.h" 

namespace ipc
{
	template <class T>
	struct result {
		T data;
		bool ok;
		result(const T& d, const bool& k);
	};

	template <class T>
	result<T>::result(const T& d, const bool& k)
		: data(d)
		, ok(k)
	{
	}

	struct channable
	{
		virtual void* peek(void) = 0;
		virtual bool poke(void* data) = 0;
		virtual void add_sender(const std::shared_ptr<context>& ctext) = 0;
		virtual void add_receiver(const std::shared_ptr<context>& ctext) = 0;
		virtual bool remove_sender(const std::shared_ptr<context>& ctext) = 0;
		virtual bool remove_receiver(const std::shared_ptr<context>& ctext) = 0;
		virtual ~channable(void) {}
	};

	template <class T>
	class channel : public channable, public noncopyable
	{
		std::shared_ptr<T[]> buffer_;
		int size_;

		std::vector<std::shared_ptr<context>> recvq_;
		std::vector<std::shared_ptr<context>> sendq_;

		std::atomic_bool closed_;
		std::atomic_int count_;

		std::size_t sendx_;
		std::size_t recvx_;
	public:
		channel(int size = 0);
	public:
		std::size_t capacity(void) const;
		std::size_t size(void) const;
		bool empty(void) const;
	public:
		bool send(const T& data, const bool& block = true);
	public:
		result<T> recv(const bool& block = true);
	public:
		void close(void);
	public:
		void add_sender(const std::shared_ptr<context>& ctext);
		void add_receiver(const std::shared_ptr<context>& ctext);
	public:
		bool remove_sender(const std::shared_ptr<context>& ctext);
		bool remove_receiver(const std::shared_ptr<context>& ctext);
	public:
		void* peek(void);
		bool poke(void* data);
	private:
		bool dispatch(const T& data, const bool& block);
		result<T> receive(const bool& block);
	};

	template <class T>
	channel<T>::channel(int size)
		: closed_(false)
		, count_(0)
		, sendx_(0)
		, recvx_(0)
		, size_(size)
		, buffer_(new T[size], std::default_delete<T[]>())
	{
	}

	template <class T>
	std::size_t channel<T>::capacity(void) const
	{
		return size_;
	}

	template <class T>
	std::size_t channel<T>::size(void) const
	{
		return count_;
	}

	template <class T>
	bool channel<T>::empty(void) const
	{
		return size() == 0;
	}

	template <class T>
	bool channel<T>::send(const T& data, const bool& block)
	{
		if (!block && ((capacity() == 0 && recvq_.empty()) ||
			(capacity() > 0 && size() == capacity())) && !closed_)
			return false;
		std::lock_guard<std::mutex> lock(context::mutex);
		return dispatch(data, block);
	}

	template <class T>
	result<T> channel<T>::recv(const bool& block)
	{
		if (!block && ((capacity() == 0 && sendq_.empty()) ||
			(capacity() > 0 && size() == 0)) && !closed_)
			return result<T>(T(), false);
		std::lock_guard<std::mutex> lock(context::mutex);
		return receive(block);
	}

	template <class T>
	void channel<T>::close(void)
	{
		std::lock_guard<std::mutex> lock(context::mutex);
		if (!closed_)
		{
			closed_ = true;
			for (auto q: recvq_)
				q->signal();
			for (auto q: sendq_)
				q->signal();
		}
	}

	template <class T>
	void channel<T>::add_sender(const std::shared_ptr<context>& ctext)
	{
		sendq_.push_back(ctext);
	}

	template <class T>
	void channel<T>::add_receiver(const std::shared_ptr<context>& ctext)
	{
		recvq_.push_back(ctext);
	}

	template <class T>
	bool channel<T>::remove_sender(const std::shared_ptr<context>& ctext)
	{
		auto it = std::find(sendq_.begin(), sendq_.end(), ctext);
		if (it != sendq_.end())
			sendq_.erase(it);
		else
			return false;
		return true;
	}

	template <class T>
	bool channel<T>::remove_receiver(const std::shared_ptr<context>& ctext)
	{
		auto it = std::find(recvq_.begin(), recvq_.end(), ctext);
		if (it != recvq_.end())
			recvq_.erase(it);
		else
			return false;
		return true;
	}

	template <class T>
	void* channel<T>::peek(void)
	{
		result<T> res = recv(false);
		return res.ok ? new T(res.data) : nullptr;
	}

	template <class T>
	bool channel<T>::poke(void* data)
	{
		return send(*static_cast<T*>(data), false);
	}

	template <class T>
	bool channel<T>::dispatch(const T& data, const bool& block)
	{
		while (true)
		{
			if (closed_)
				throw std::runtime_error("send on closed channel");
			if (!recvq_.empty() && size() > 0)
			{
				std::shared_ptr<context> ctext = recvq_.front();
				sendq_.erase(recvq_.begin());
				ctext->unblocked_receiver(this, new T(buffer_.get()[recvx_]));
				if (++recvx_ >= capacity())
					recvx_ = 0;
				count_++;
				ctext->signal();
			}
			if (!recvq_.empty())
			{
				std::shared_ptr<context> ctext = recvq_.front();
				recvq_.erase(recvq_.begin());
				ctext->unblocked_receiver(this, new T(data));
				ctext->signal();
				return true;
			}
			if (size() < capacity())
			{
				buffer_.get()[sendx_] = data;
				sendx_++;
				if (sendx_ == capacity())
					sendx_ = 0;
				count_++;
				return true;
			}
			if (!block)
				return false;
			std::shared_ptr<context> ctext = context::get();
			ctext->add(this, new T(data));
			sendq_.push_back(ctext);
			try
			{
				ctext->wait();
			}
			catch (...)
			{
				auto it = std::find(sendq_.begin(), sendq_.end(), ctext);
				if (it != sendq_.end())
					sendq_.erase(it);
				ctext->clear();
				return true;
			}
			auto it = std::find(sendq_.begin(), sendq_.end(), ctext);
			if (it != sendq_.end())
			{
				ctext->clear();
				throw std::runtime_error("not removed from send queue");
			}
			if (ctext->get_unblocked_index() != -1)
			{
				ctext->clear();
				return true;
			}
			ctext->clear();
		}
	}

	template <class T>
	result<T> channel<T>::receive(const bool& block)
	{
		while (true)
		{
			if (closed_ && size() == 0)
				return result<T>(T(), true);	// todo
			T data;
			bool has_data(false);
			if (size() > 0)
			{
				data = buffer_.get()[recvx_];
				recvx_++;
				if (recvx_ == capacity())
					recvx_ = 0;
				count_--;
				has_data = true;
			}
			if (!has_data && !sendq_.empty())
			{
				std::shared_ptr<context> ctext = sendq_.front();
				sendq_.erase(sendq_.begin());
				void* pd = ctext->unblocked_sender(this);
				data = *static_cast<T*>(pd);
				delete pd;
				has_data = true;
				ctext->signal();
			}
			if (!sendq_.empty() && size() < capacity())
			{
				std::shared_ptr<context> ctext = sendq_.front();
				sendq_.erase(sendq_.begin());
				void* pd = ctext->unblocked_sender(this);
				buffer_.get()[sendx_] = *static_cast<T*>(pd);
				delete pd;
				if (++sendx_ >= capacity())
					sendx_ = 0;
				count_++;
				ctext->signal();
			}
			if (has_data)
				return result<T>(data, true);
			std::shared_ptr<context> ctext = context::get();
			ctext->add(this);
			recvq_.push_back(ctext);
			try
			{
				ctext->wait();
			}
			catch (...)
			{
				auto it = std::find(recvq_.begin(), recvq_.end(), ctext);
				if (it != recvq_.end())
					recvq_.erase(it);
				ctext->clear();
				return result<T>(T(), false);
			}
			auto it = std::find(recvq_.begin(), recvq_.end(), ctext);
			if (it != recvq_.end())
			{
				ctext->clear();
				throw std::runtime_error("not removed from receive queue");
			}
			if (ctext->get_unblocked_index() != -1)
			{
				void* pd = ctext->get_receive_data();
				data = *static_cast<T*>(pd);
				delete pd;
				ctext->clear();
				return result<T>(data, true);
			}
			ctext->clear();
		}
	}
}

#endif
