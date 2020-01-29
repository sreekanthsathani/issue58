#include <vector>
#include <string>
#include "socket_header.h"
#include "Interface/Thread.h"
#include "Interface/Pipe.h"
#include "Server.h"

class CServiceWorker;
class IService;

class CServiceAcceptor : public IThread
{
public:
	CServiceAcceptor(IService * pService, std::string pName, unsigned short port, int pMaxClientsPerThread, IServer::BindTarget bindTarget);
	~CServiceAcceptor();

	void operator()(void);

private:
	void AddToWorker(SOCKET pSocket, const std::string& endpoint);

	std::vector<CServiceWorker*> workers;
	SOCKET s;

	std::string name;

	IService * service;
	IPipe *exitpipe;
#ifndef _WIN32
	int xpipe[2];
#endif

	bool do_exit;

	bool has_error;

	int maxClientsPerThread;
};
