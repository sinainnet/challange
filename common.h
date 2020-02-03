#ifndef COMMON_H_
#define COMMON_H_

static constexpr std::size_t BUF_SIZE = 32000;	// Near the maximum
static constexpr std::size_t DEFUALT_WAIT = 5;	// Near the maximum

class communication
{
public:
	virtual int connect() = 0;
	virtual int send(char*, size_t) = 0;
	virtual int receive(char*, size_t) = 0;
	virtual int disconnect() = 0;
};

#endif
