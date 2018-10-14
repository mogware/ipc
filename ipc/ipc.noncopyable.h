#ifndef __IPC_NONCOPYABLE__
#define __IPC_NONCOPYABLE__

namespace ipc
{
	class noncopyable
	{
	protected:
		constexpr noncopyable() = default;
		~noncopyable() = default;
	protected:
		noncopyable(const noncopyable&) = delete;
		noncopyable(noncopyable&&) = delete;
	protected:
		noncopyable & operator=(const noncopyable&) = delete;
		noncopyable& operator=(noncopyable&&) = delete;
	};
}

#endif
