#include "ipc.selector.h"

ipc::selector::selector(void)
	: data_(nullptr)
{
}

ipc::selector::~selector(void)
{
	set_data(nullptr);
}

void ipc::selector::clear(void)
{
	send_data_.clear();
}

int ipc::selector::select(const bool& block)
{
	std::shared_ptr<context> ctext = context::get();
	for (auto sd: send_data_)
		ctext->add(sd.first, sd.second);

	while (true)
	{
		std::size_t size = ctext->send_data_size();
		std::size_t i = size < 1 ? 0 : std::rand() % size;
		for (std::size_t n = 0; n < size; n++)
		{
			channable* ch = ctext->send_data_channel(i);
			if (ch != nullptr)
			{
				void* data = ctext->send_data_data(i);
				if (data == nullptr)
				{
					void* peek = ch->peek();
					if (peek != nullptr)
					{
						ctext->clear();
						set_data(peek);
						return i;
					}
				}
				else
				{
					bool dont_block = ch->poke(data);
					if (dont_block)
					{
						ctext->clear();
						set_data(nullptr);
						return i;
					}
				}
			}
			if (++i >= size)
				i = 0;
		}

		if (!block)
			return -1;

		std::lock_guard<std::mutex> lock(context::mutex);
		ctext->add_to_all_channels();
		try
		{
			ctext->wait();
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
			continue;
		}
		if (ctext->send_data_size() > 0)
		{
			ctext->clear();
			throw std::runtime_error("channel exist");
		}
		int index = ctext->get_unblocked_index();
		if (index == -1)
		{
			ctext->clear();
			throw std::runtime_error("illegal state");
		}
		set_data(ctext->get_receive_data());
		ctext->clear();
		return index;
	}
}

void ipc::selector::set_data(void* data)
{
	if (data_ != nullptr)
		delete data_;
	data_ = data;
}