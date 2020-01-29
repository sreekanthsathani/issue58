#include "Interface/Pipe.h"
#include "socket_header.h"
#include <vector>

class CStreamPipe : public IPipe
{
public:
	CStreamPipe( SOCKET pSocket);
	~CStreamPipe();

	virtual size_t Read(char *buffer, size_t bsize, int timeoutms);
	virtual bool Write(const char *buffer, size_t bsize, int timeoutms, bool flush);
	virtual size_t Read(std::string *ret, int timeoutms);
	virtual bool Write(const std::string &str, int timeoutms, bool flush);

	virtual bool isWritable(int timeoutms);
	virtual bool isReadable(int timeoutms);

	virtual bool hasError(void);

	virtual void shutdown(void);

	virtual size_t getNumElements(void){ return 0;};

	SOCKET getSocket(void);

	virtual void addThrottler(IPipeThrottler *throttler);
	virtual void addOutgoingThrottler(IPipeThrottler *throttler);
	virtual void addIncomingThrottler(IPipeThrottler *throttler);

	virtual _i64 getTransferedBytes(void);
	virtual void resetTransferedBytes(void);

	virtual bool Flush( int timeoutms=-1 );

private:
	SOCKET s;
	bool doThrottle(size_t new_bytes, bool outgoing, bool wait);

	_i64 transfered_bytes;

	bool has_error;

	std::vector<IPipeThrottler*> incoming_throttlers;
	std::vector<IPipeThrottler*> outgoing_throttlers;
};
