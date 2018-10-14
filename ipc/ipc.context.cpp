#include "ipc.channel.h"
#include "ipc.context.h"

#include <stdexcept>
#include <cstdlib>
#include <ctime>

ipc::threadvar<ipc::context> ipc::context::context_;
std::mutex ipc::context::mutex;

ipc::context::context(void)
	: recv_data_(nullptr)
	, count_(0)
	, unblockedx_(-1)
{
	std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

ipc::context::~context(void)
{
}

std::shared_ptr<ipc::context> ipc::context::get(void)
{
	return context_.get();
}

void ipc::context::add(ipc::channable* chan, void* data)
{
	send_data_.push_back(std::make_pair(chan, data));
}

void ipc::context::add(ipc::channable* chan)
{
	send_data_.push_back(std::make_pair(chan, nullptr));
}

void ipc::context::add_to_all_channels(void)
{
	std::size_t size = send_data_.size();
	for (std::size_t i = 0; i < size; i++)
	{
		channable* ch = send_data_[i].first;
		if (ch == nullptr)
			continue;
		if (send_data_[i].second == nullptr)
			ch->add_receiver(this->shared_from_this());
		else
			ch->add_sender(this->shared_from_this());
	}
}

void ipc::context::remove_from_all_channels(void)
{
	std::size_t size = send_data_.size();
	for (std::size_t i = 0; i < size; i++)
	{
		channable* ch = send_data_[i].first;
		if (ch == nullptr)
			continue;
		if (send_data_[i].second == nullptr)
			ch->remove_receiver(this->shared_from_this());
		else
			ch->remove_sender(this->shared_from_this());
	}
}

void ipc::context::clear(void)
{
	unblockedx_ = -1;
	recv_data_ = nullptr;
	send_data_.clear();
}

int ipc::context::get_unblocked_index(void) const
{
	return unblockedx_;
}

void* ipc::context::get_receive_data(void) const
{
	return recv_data_;
}

void* ipc::context::unblocked_sender(ipc::channable* chan)
{
	void* data = nullptr;
	bool found = false;
	std::size_t size = send_data_.size();
	std::size_t i = std::rand() % size;
	for (std::size_t n = 0; n < size; n++)
	{
		channable* ch = send_data_[i].first;
		if (ch != nullptr)
		{
			if (ch == chan && send_data_[i].second != nullptr && !found)
			{
				unblockedx_ = static_cast<int>(i);
				data = send_data_[i].second;
				found = true;
			}
			else if (send_data_[i].second == nullptr)
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

void ipc::context::unblocked_receiver(ipc::channable* chan, void* data)
{
	bool found = false;
	std::size_t size = send_data_.size();
	std::size_t i = std::rand() % size;
	for (std::size_t n = 0; n < size; n++)
	{
		channable* ch = send_data_[i].first;
		if (ch != nullptr)
		{
			if (ch == chan && send_data_[i].second == nullptr && !found)
			{
				unblockedx_ = static_cast<int>(i);
				recv_data_ = data;
				found = true;
			}
			else if (send_data_[i].second == nullptr)
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

void ipc::context::signal(void)
{
	std::unique_lock<std::mutex> lock(mutex, std::defer_lock);
	++count_;
	cond_.notify_one();
}

void ipc::context::wait(void)
{
	std::unique_lock<std::mutex> lock(mutex, std::defer_lock);
	while (!count_)
		cond_.wait(lock);
	--count_;
}

std::size_t ipc::context::send_data_size(void) const
{
	return 	send_data_.size();
}

ipc::channable* ipc::context::send_data_channel(const int& i) const
{
	return send_data_[i].first;
}

void* ipc::context::send_data_data(const int& i) const
{
	return send_data_[i].second;
}
