#ifndef __IPC__
#define __IPC__

#include <atomic>
#include <memory>
#include <vector>
#include <array>
#include <thread>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <stdexcept>

#include <cstdlib>
#include <ctime>

namespace ipc
{
	template <class T, std::size_t N>
	class context;

	template <class T>
	struct result {
		T data;
		bool ok;
		result(T d, bool k);
	};

	template <class T>
	result<T>::result(T d, bool k)
		: data(d)
		, ok(k)
	{
	}

	template <class T, std::size_t N>
	class channel
	{
		std::array<T,N> buffer_;
		std::vector<std::shared_ptr<context<T,N>>> recvq_;
		std::vector<std::shared_ptr<context<T,N>>> sendq_;

		std::atomic_bool closed_;
		std::atomic_int count_;

		std::size_t sendx_;
		std::size_t recvx_;

		std::mutex mutex_;
	public:
		channel(void);
	public:
		constexpr std::size_t capacity(void) const;
		std::size_t size(void) const;
		bool empty(void) const;
	public:
		bool send(T data, bool block = true);
	public:
		result<T> recv(bool block = true);
	public:
		void close(void);
	public:
		bool remove_sender(std::shared_ptr<context<T,N>> ctext);
		bool remove_receiver(std::shared_ptr<context<T,N>> ctext);
	private:
		bool dispatch(T data, bool block);
		result<T> receive(bool block);
	};

	template <class T, std::size_t N>
	channel<T,N>::channel(void)
		: closed_(false)
		, count_(0)
		, sendx_(0)
		, recvx_(0)
	{
	}

	template <class T, std::size_t N>
	constexpr std::size_t channel<T,N>::capacity(void) const
	{
		return N;
	}

	template <class T, std::size_t N>
	std::size_t channel<T,N>::size(void) const
	{
		return count_;
	}

	template <class T, std::size_t N>
	bool channel<T,N>::empty(void) const
	{
		return size() == 0;
	}

	template <class T, std::size_t N>
	bool channel<T,N>::send(T data, bool block)
	{
		if (!block && ((capacity() == 0 && recvq_.empty()) ||
				(capacity() > 0 && size() == capacity())) && !closed_)
			return false;
		std::lock_guard<std::mutex> lock(mutex_);
		return dispatch(data, block);
	}

	template <class T, std::size_t N>
	result<T> channel<T,N>::recv(bool block)
	{
		if (!block && ((capacity() == 0 && sendq_.empty()) ||
				(capacity() > 0 && size() == 0)) && !closed_)
			return result<T>(T(), false);
		std::lock_guard<std::mutex> lock(mutex_);
		return receive(block);
	}

	template <class T, std::size_t N>
	void channel<T,N>::close(void)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!closed_)
		{
			closed_ = true;
			for (auto q: recvq_)
				q->signal(mutex_);
			for (auto q : sendq_)
				q->signal(mutex_);
		}
	}

	template <class T, std::size_t N>
	bool channel<T,N>::remove_sender(std::shared_ptr<context<T,N>> ctext)
	{
		auto it = std::find(sendq_.begin(), sendq_.end(), ctext);
		if (it != sendq_.end())
			sendq_.erase(it);
		else
			return false;
		return true;
	}

	template <class T, std::size_t N>
	bool channel<T,N>::remove_receiver(std::shared_ptr<context<T,N>> ctext)
	{
		auto it = std::find(recvq_.begin(), recvq_.end(), ctext);
		if (it != recvq_.end())
			recvq_.erase(it);
		else
			return false;
		return true;
	}

	template <class T, std::size_t N>
	bool channel<T,N>::dispatch(T data, bool block)
	{
		while (true)
		{
			if (closed_)
				throw std::runtime_error("send on closed channel");
			if (!recvq_.empty() && size() > 0)
			{
				std::shared_ptr<context<T, N>> ctext = recvq_.front();
				sendq_.erase(recvq_.begin());
				ctext->unblocked_receiver(this, buffer_[recvx_]);
				if (++recvx_ >= capacity())
					recvx_ = 0;
				count_++;
				ctext->signal(mutex_);
			}
			if (!recvq_.empty())
			{
				std::shared_ptr<context<T, N>> ctext = recvq_.front();
				recvq_.erase(recvq_.begin());
				ctext->unblocked_receiver(this, data);
				ctext->signal(mutex_);
				return true;
			}
			if (size() < capacity())
			{
				buffer_[sendx_] = data;
				sendx_++;
				if (sendx_ == capacity())
					sendx_ = 0;
				count_++;
				return true;
			}
			if (!block)
				return false;
			std::shared_ptr<context<T,N>> ctext = context<T,N>::get();
			ctext->add(this, data);
			sendq_.push_back(ctext);
			try
			{
				ctext->wait(mutex_);
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

	template <class T, std::size_t N>
	result<T> channel<T,N>::receive(bool block)
	{
		while (true)
		{
			if (closed_ && size() == 0)
				return result<T>(T(), false);
			T data;
			bool has_data(false);
			if (size() > 0)
			{
				data = buffer_[recvx_];
				recvx_++;
				if (recvx_ == capacity())
					recvx_ = 0;
				count_--;
				has_data = true;
			}
			if (!has_data && !sendq_.empty())
			{
				std::shared_ptr<context<T,N>> ctext = sendq_.front();
				sendq_.erase(sendq_.begin());
				data = ctext->unblocked_sender(this);
				has_data = true;
				ctext->signal(mutex_);
			}
			if (!sendq_.empty() && size() < capacity())
			{
				std::shared_ptr<context<T, N>> ctext = sendq_.front();
				sendq_.erase(sendq_.begin());
				buffer_[sendx_] = ctext->unblocked_sender(this);
				if (++sendx_ >= capacity())
					sendx_ = 0;
				count_++;
				ctext->signal(mutex_);
			}
			if (has_data)
				return result<T>(data, true);
			std::shared_ptr<context<T, N>> ctext = context<T, N>::get();
			ctext->add(this);
			recvq_.push_back(ctext);
			try
			{
				ctext->wait(mutex_);
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
				data = ctext->get_receive_data();
				ctext->clear();
				return result<T>(data, true);
			}
			ctext->clear();
		}
	}

	template <class T>
	class thread_private
	{
		std::mutex mutex_;
		std::unordered_map<std::thread::id, std::shared_ptr<T>> threads_;
	public:
		std::shared_ptr<T> get(void)
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
	};

	template <class T, std::size_t N>
	class context : public std::enable_shared_from_this<context<T, N>>
	{
		static thread_private<context<T,N>> context_;
		std::condition_variable cond_;
		unsigned long count_ = 0;

		int unblockedx_;
		T recv_data_;

		std::vector<std::tuple<channel<T, N>*, T, bool>> send_data_;
	public:
		context(void);
	public:
		static std::shared_ptr<context<T,N>> get(void);
	public:
		void add(channel<T,N>* chan, T data);
		void add(channel<T,N>* chan);
	public:
		void clear(void);
	public:
		int get_unblocked_index(void) const;
	public:
		T get_receive_data(void);
	public:
		T unblocked_sender(channel<T,N>* chan);
		void unblocked_receiver(channel<T, N>* chan, T data);
	public:
		void signal(std::mutex& m);
		void wait(std::mutex& m);
	};

	template <class T, std::size_t N>
	thread_private<context<T,N>> context<T,N>::context_;

	template <class T, std::size_t N>
	context<T,N>::context(void)
	{
		std::srand(static_cast<unsigned int>(std::time(nullptr)));
	}

	template <class T, std::size_t N>
	std::shared_ptr<context<T,N>> context<T,N>::get(void)
	{
		return context_.get();
	}

	template <class T, std::size_t N>
	void context<T,N>::add(channel<T,N>* chan, T data)
	{
		send_data_.push_back(std::make_tuple(chan, data, false));
	}

	template <class T, std::size_t N>
	void context<T,N>::add(channel<T,N>* chan)
	{
		send_data_.push_back(std::make_tuple(chan, T(), true));
	}
	
	template <class T, std::size_t N>
	void context<T,N>::clear(void)
	{
		unblockedx_ = -1;
		recv_data_ = T();
		send_data_.clear();
	}

	template <class T, std::size_t N>
	int context<T,N>::get_unblocked_index(void) const
	{
		return unblockedx_;
	}

	template <class T, std::size_t N>
	T context<T, N>::get_receive_data(void)
	{
		return recv_data_;
	}

	template <class T, std::size_t N>
	T context<T,N>::unblocked_sender(channel<T,N>* chan)
	{
		T data;
		bool found = false;
		std::size_t size = send_data_.size();
		int i = std::rand() % size;
		for (int n = 0; n < size; n++)
		{
			channel<T,N>* ch = std::get<0>(send_data_[i]);
			if (ch != nullptr)
			{
				if (ch == chan && !std::get<2>(send_data_[i]) && !found)
				{
					unblockedx_ = i;
					data = std::get<1>(send_data_[i]);
					found = true;
				}
				else if (std::get<2>(send_data_[i]))
					ch->remove_receiver(this->shared_from_this());
				else
					ch->remove_sender(this->shared_from_this());
			}
			if (++i >= size)
				i = 0;
		}
		if (!found)
			throw std::runtime_error("chan not found in context");
		send_data_.clear();
		return data;
	}

	template <class T, std::size_t N>
	void context<T, N>::unblocked_receiver(channel<T, N>* chan, T data)
	{
		bool found = false;
		std::size_t size = send_data_.size();
		int i = std::rand() % size;
		for (int n = 0; n < size; n++)
		{
			channel<T, N>* ch = std::get<0>(send_data_[i]);
			if (ch != nullptr)
			{
				if (ch == chan && std::get<2>(send_data_[i]) && !found)
				{
					unblockedx_ = i;
					recv_data_ = data;
					found = true;
				}
				else if (std::get<2>(send_data_[i]))
					ch->remove_receiver(this->shared_from_this());
				else
					ch->remove_sender(this->shared_from_this());
			}
			if (++i >= size)
				i = 0;
		}
		if (!found)
			throw std::runtime_error("chan not found in context");
		send_data_.clear();
	}

	template <class T, std::size_t N>
	void context<T,N>::signal(std::mutex& m)
	{
		std::unique_lock<std::mutex> lock(m, std::defer_lock);
		++count_;
		cond_.notify_one();
	}

	template <class T, std::size_t N>
	void context<T,N>::wait(std::mutex& m)
	{
		std::unique_lock<std::mutex> lock(m, std::defer_lock);
		while (!count_)
			cond_.wait(lock);
		--count_;
	}

	template <class T, std::size_t N>
	class selector
	{
		std::vector<std::tuple<channel<T, N>*, T, bool>> send_data_;
		T data_;
		std::mutex mutex_;
	public:
		selector(void);
	public:
		selector<T,N>& send(channel<T,N>* chan, T data);
		selector<T,N>& recv(channel<T,N>* chan);
	public:
		int select(bool block = true);
	public:
		T get_data(void) const;
	private:
		int chan_select(std::shared_ptr<context<T, N>> ctext);
	};

	template <class T, std::size_t N>
	selector<T,N>::selector(void)
	{
		std::srand(static_cast<unsigned int>(std::time(nullptr)));
	}

	template <class T, std::size_t N>
	selector<T,N>& selector<T,N>::send(channel<T,N>* chan, T data)
	{
		send_data_.push_back(std::make_tuple(chan, data, false));
		return *this;
	}

	template <class T, std::size_t N>
	selector<T,N>& selector<T,N>::recv(channel<T,N>* chan)
	{
		send_data_.push_back(std::make_tuple(chan, T(), true));
		return *this;
	}

	template <class T, std::size_t N>
	int selector<T,N>::select(bool block)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		std::shared_ptr<context<T, N>> ctext = context<T, N>::get();
		ctext->set_send_data(send_data_);
		send_data_.clear();
		while (true)
		{
			int index = chan_select(ctext);
			if (index >= 0)
			{
				ctext->clear();
				return index;
			}
			if (!block)
			{
				ctext->clear();
				return -1;
			}
			ctext->add_to_all_channels();
			try
			{
				ctext->wait(mutex_);
			}
			catch (...)
			{
				ctext->remove_from_all_channels();
				ctext->clear();
				return -1;
			}
			if (ctext->get_unblocked_index() == -1)
			{
				ctext->remove_from_all_channels();
				ctext->clear();
				continue;
			}
			if (!ctext->empty())
			{
				ctext->clear();
				throw std::runtime_error("channel exist");
			}
			int targetIndex = ctext->get_unblocked_index();
			if (targetIndex == -1)
			{
				ctext->clear();
				throw std::runtime_error("illegal state");
			}
			data = ctext->get_receive_data();
			ctext->clear();
			return targetIndex;
		}
	}

	template <class T, std::size_t N>
	T selector<T, N>::get_data(void) const
	{
		return data_;
	}

	template <class T, std::size_t N>
	int selector<T, N>::chan_select(std::shared_ptr<context<T,N>> ctext)
	{
		std::size_t size = send_data_.size();
		int i = size < 1 ? 0 : std::rand() % size;
		for (int n = 0; n < size; n++)
		{
			channel<T, N>* ch = ctext->get_channel(i);
			if (ch != nullptr)
			{
				if (ctext->get_recv_flag(i))
				{
					result<T> peek = ch->recv(false);
					if (peek.ok)
					{
						data_ = peek.data;
						return i;
					}
				}
				else
				{
					T data = ctext->get_send_data(i);
					bool dont_block = ch > send(data, false);
					if (dont_block)
					{
						data_ = T();
						return i;
					}
				}
			}
			if (++i >= size)
				i = 0;
		}
		return -1;
	}

	class ticker
	{
	};
}

#endif
