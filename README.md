# Class library for a golang like implementaion of channels

## Usage

Example of producer/consumer using channels

```c
void producer(ipc::channel<std::string>* ch)
{
	for (int n = 1; n < 20; n++)
	{
		ch->send(std::to_string(n));
		Sleep(1000);
	}
	ch->send(std::to_string(0));
}
```

```c
void consumer(ipc::channel<std::string>* ch)
{
	while (true)
	{
		ipc::selector sel;
		sel.recv(*ch);
		if (sel.select() == -1)
			continue;
		std::string data = sel.get_data<std::string>();
		if (data.compare("0") == 0)
			break;
		std::printf("recv: %s\n", data.c_str());
		Sleep(1000);
	}
}
```

```c
int main()
{
	ipc::channel<std::string> ch(3);

	std::thread t1(producer, &ch);
	std::thread t2(consumer, &ch);

	t1.join();
	t2.join();

	ch.close();

    return 0;
}
```
