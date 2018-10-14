#include "ipc.ticker.h"

ipc::ticker::ticker(const std::chrono::system_clock::duration& d)
	: c(1)
	, runner_(std::bind(&scheduler::run, &timer_))
{
	timer_.schedule(std::bind(&ticker::send_time, this), d, d);
}

ipc::ticker::~ticker(void)
{
	if (runner_.joinable())
		stop();
}

void ipc::ticker::stop(void)
{
	timer_.stop();
	runner_.join();
}

void ipc::ticker::send_time(void)
{
	c.send(true, false);
}
