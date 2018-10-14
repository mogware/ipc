#include "ipc.scheduler.h"

ipc::scheduler::scheduler(void)
	: nthreads_(0)
	, stop_requested_(false)
	, stop_when_empty_(false)
{
}

ipc::scheduler::~scheduler(void)
{
}

void ipc::scheduler::run(void)
{
	std::unique_lock<std::mutex> lock(mutex_);

	nthreads_++;
	stop_requested_ = false;
	stop_when_empty_ = false;

	while (!stop_requested_ && !(stop_when_empty_ && tasks_.empty()))
	{
		try
		{
			while (!stop_requested_ && !stop_when_empty_ && tasks_.empty())
				cond_.wait(lock);
			std::chrono::system_clock::time_point t = tasks_.begin()->first;
			while (!stop_requested_ && !tasks_.empty() &&
					cond_.wait_until(lock, t) != std::cv_status::timeout)
				; // keep waiting until timeout
			if (stop_requested_)
				break;
			if (tasks_.empty())
				continue;
			func f = tasks_.begin()->second;
			tasks_.erase(tasks_.begin());
			lock.unlock();
			f();
			lock.lock();
		}
		catch (...)
		{
			nthreads_--;
			throw;
		}
	}
	nthreads_--;
}

void ipc::scheduler::stop(const bool& drain)
{
	{
		std::unique_lock<std::mutex> lock(mutex_);
		if (drain)
			stop_when_empty_ = true;
		else
			stop_requested_ = true;
	}
	cond_.notify_all();
}

void ipc::scheduler::schedule(const ipc::func& f,
	const std::chrono::system_clock::time_point& t)
{
	{
		std::unique_lock<std::mutex> lock(mutex_);
		tasks_.insert(std::make_pair(t, f));
	}
	cond_.notify_one();
}

void ipc::scheduler::schedule(const ipc::func& f,
	const std::chrono::system_clock::duration& s)
{
	schedule(f, std::chrono::system_clock::now() + s);
}

static void repeat(ipc::scheduler* s, ipc::func f,
	const std::chrono::system_clock::duration& d)
{
	f(); s->schedule(std::bind(&repeat, s, f, d), d);
}

void ipc::scheduler::schedule(const ipc::func& f,
	const std::chrono::system_clock::duration& s,
	const std::chrono::system_clock::duration& d)
{
	schedule(std::bind(&repeat, this, f, d), s);
}
