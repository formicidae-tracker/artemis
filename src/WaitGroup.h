#pragma once


class WaitGroup {
public:
	WaitGroup();
	~WaitGroup();

	void Add(unsigned int i);
	void Done();
	void Wait();

private :
	void add(int i);
};
