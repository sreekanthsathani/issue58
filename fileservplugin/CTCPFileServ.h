#pragma warning ( disable:4005 )
#pragma warning ( disable:4996 )

#include "types.h"
#include <string>
#include <vector>

class CClientThread;
class CUDPThread;

#include "socket_header.h"

#include "CriticalSection.h"
#include "../Interface/ThreadPool.h"

class CTCPFileServ
{
public:
	CTCPFileServ(void);
	~CTCPFileServ(void);
	void KickClients();
	bool Start(_u16 tcpport,_u16 udpport, std::string pServername, bool use_fqdn);

	bool Run(void);

	_u16 getUDPPort();
	_u16 getTCPPort();
	std::string getServername();
	bool getUseFQDN();

private:
	bool TcpStep(void);
	void DelClientThreads(void);

	SOCKET mSocket;

	std::vector<CClientThread*> clientthreads;
	CUDPThread *udpthread;

	CriticalSection cs;
	_u16 m_tcpport;
	_u16 m_udpport;

	THREADPOOL_TICKET udpticket;

	bool m_use_fqdn;
};

extern CTCPFileServ *TCPServer;

