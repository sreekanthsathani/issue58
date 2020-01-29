/*************************************************************************
*    UrBackup - Client/Server backup system
*    Copyright (C) 2011-2016 Martin Raiber
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU Affero General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "vld.h"
#define DEF_SERVER
#ifdef _WIN32
#include <ws2tcpip.h>
#endif
#include "Server.h"
#include "AcceptThread.h"
#include "SessionMgr.h"
#include "LoadbalancerClient.h"
#include "libs.h"
#include "sqlite/sqlite3.h"
#include "stringtools.h"
#include <iostream>
#include <vector>
#include <map>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <signal.h>
#endif
#ifdef AS_SERVICE
#	include "win_service/nt_service.h"
using namespace nt;
#endif

#ifndef _WIN32
#	include <sys/types.h>
#	include <pwd.h>
#	include <sys/wait.h>
#	include <grp.h>
#	include <errno.h>
#endif

CServer *Server=NULL;

using namespace std;

bool run=true;
bool no_server=false;
namespace
{
    std::string g_logfile;
    std::string g_logfile_user;
}

void init_mutex_selthread(void);
void destroy_mutex_selthread(void);

#ifndef _WIN32
void termination_handler(int signum)
{
	run=false;
	Server->Log("Shutting down (Signal "+convert(signum)+")", LL_WARNING);
}

void hub_handler(int signum)
{
	if(!g_logfile.empty())
	{
		Server->setLogFile(g_logfile, g_logfile_user);
	}
}
#else
void abort_handler(int signal)
{
	Server->Log("Program abort (SIGABRT)", LL_ERROR);
	RaiseException(0, 0, 0, NULL);
}

void invalid_parameter_handler(const wchar_t* expression,
	const wchar_t* function,
	const wchar_t* file,
	unsigned int line,
	uintptr_t pReserved)
{
	Server->Log("Invalid parameter detected in function " + Server->ConvertFromWchar(function) +
		" File: " + Server->ConvertFromWchar(file) + " Line: " + convert(line) + " Expression: " + Server->ConvertFromWchar(expression), LL_ERROR);
	RaiseException(0, 0, 0, NULL);
}

#endif

#ifdef AS_SERVICE
char **srv_argv;
int srv_argc;
#endif

void DisplayHelp(void)
{
	cout << "CServer - Compiled C++ Server" << endl;
	cout << "CServer [--port 34255] [--workers 3] [--plugin x] [--loadbalancer 192.168.0.1] [--lb-weight 1] [--lb-port 2304]" << endl;
}

CAcceptThread *c_at=NULL;

int main_fkt(int argc, char *argv[]);

#ifndef AS_SERVICE

#ifdef _WIN32
#define MAINNAME main
#else
#define MAINNAME real_main
#endif

int MAINNAME(int argc, char *argv[])
{
#else //AS_SERVICE
int my_init_fcn_t(int argc, char *argv[])
{
#endif
	#ifdef _WIN32
#ifndef _DEBUG
	__try{
#endif
#endif
	return main_fkt(argc, argv);
#ifdef _WIN32
#ifndef _DEBUG
	}__except(CServer::WriteDump(GetExceptionInformation()) )
	{
	}
	return 101;
#endif
return 101;
#endif
}

int main_fkt(int argc, char *argv[])
{
	Server=new CServer;
	Server->setup();

#ifdef _WIN32
	int rc;

	WSADATA wsadata;
	rc = WSAStartup(MAKEWORD(2,2), &wsadata);
	if(rc == SOCKET_ERROR)
	{
		Server->Log("Error starting Winsock 2.2", LL_ERROR);
		return 23;
	}
#endif

	srand(Server->getSecureRandomNumber());

	//Parse Parameters
	
	int port=34255;
	int workers=5;
	
	std::vector<std::string> plugins;
	
	std::string loadbalancer;
	int loadbalancer_weight=1;
	int loadbalancer_port=2305;
	
	if( argc==2 && (std::string)argv[1]=="--version" )
	{
	    cout << "CServer version 0.1.0.0" << endl;
	    return 1;
	}
	
	if( argc==2 && ( (std::string)argv[1]=="--help" || (std::string)argv[1]=="-h") )
	{
		DisplayHelp();
		return 6;
	} 

	std::map<std::string, std::string> srv_params;
	
	std::string loglevel;
	std::string logfile;
	std::string workingdir;
	bool daemon=false;
	std::string daemon_user;
	std::string pidfile;
	bool log_console_no_time=false;

	for(int i=1;i<argc;++i)
	{
		std::string carg=argv[i];
		std::string narg;
		if( i+1<argc )
			narg=argv[i+1];

		if( carg=="--daemon" || carg=="-d" )
		{
			daemon=true;
		}
		else if( carg=="--user" || carg=="-u" )
		{
			daemon_user=narg;
			++i;
		}
		else if( carg=="--workingdir" )
		{
			workingdir=narg;
			++i;
		}
		else if( carg=="--port" || carg=="-p" )
		{
			port=atoi( narg.c_str() );
			++i;
		}
		else if( carg=="--workers" || carg=="-w" )
		{
			workers=atoi( narg.c_str() );
			++i;
		} 
		else if( carg=="--plugin" || carg=="-p" || carg=="--add" || carg=="-a")
		{
			plugins.push_back( narg );		
			++i;
		}
		else if( carg=="--loadbalancer" || carg=="-lb" )
		{
			loadbalancer=narg;
			++i;
		}
		else if( carg=="--lb-weight" || carg=="-lbw" )
		{
			loadbalancer_weight=atoi( narg.c_str() );
			++i;
		}
		else if( carg=="--lb-port" || carg=="-lbp" )
		{
			loadbalancer_port=atoi( narg.c_str() );
			++i;
		}
		else if( carg=="--no-server" || carg=="-n" )
		{
			no_server=true;
		}
		else if (carg=="--loglevel" || carg=="-ll" )
		{
			loglevel=strlower(narg);
			++i;
		}
		else if (carg=="--logfile" || carg=="-lf" )
		{
			logfile=narg;
			++i;
		}
		else if( carg=="--pidfile" )
		{
			pidfile=narg;
			++i;
		}
		else if( carg=="--rotate-filesize")
		{
			Server->setLogRotationFilesize(atoi(narg.c_str()));
			++i;
		}
		else if( carg=="--rotate-numfiles")
		{
			Server->setLogRotationNumFiles(atoi(narg.c_str()));
			++i;
		}
		else if( carg=="--log_console_no_time")
		{
			log_console_no_time = true;
		}
		else
		{
			if( carg.size()>1 && carg[0]=='-' )
			{
				std::string p;
				if( carg[1]=='-' && carg.size()>2 )
					p=carg.substr(2, carg.size()-2);
				else
					p=carg.substr(1, carg.size()-1);

				std::string v;

				if( narg.size()>0 && narg[0]!='-' )
				{
					v=narg;
					++i;
				}
				else
				{
					size_t g=p.find_last_of("=");
					if(g!=std::string::npos)
					{
						v=p.substr(g+1);
						p=p.substr(0,g);
					}
					else
					{
						v="true";
					}
				}


				srv_params[p]=v;
			}
		}
	}

	Server->setServerParameters(srv_params);
	
	if(workingdir.empty())
	{
#ifdef _WIN32
#ifndef AS_SERVICE
		{
			wchar_t buf[MAX_PATH];
			GetCurrentDirectoryW(MAX_PATH, buf);
			Server->setServerWorkingDir(Server->ConvertFromWchar(buf));
		}
#else
		{
			wchar_t buf[MAX_PATH+1];
			GetModuleFileNameW(NULL, buf, MAX_PATH);
			Server->setServerWorkingDir(ExtractFilePath(Server->ConvertFromWchar(buf)));
			SetCurrentDirectoryW(Server->ConvertToWchar(ExtractFilePath(Server->ConvertFromWchar(buf))).c_str());
		}
#endif
#else
		char buf[4096];
		char* cwd = getcwd(buf, sizeof(buf));
		if(cwd==NULL)
		{
			Server->setServerWorkingDir((ExtractFilePath(argv[0])));
		}
		else
		{
			Server->setServerWorkingDir((cwd));
		}		
#endif
	}
	else
	{
		Server->setServerWorkingDir((workingdir));
#ifndef _WIN32
		int rc = chdir((Server->getServerWorkingDir()).c_str());
		if(rc!=0)
		{
			Server->Log("Cannot set working directory to directory "+workingdir, LL_ERROR);
		}

#endif
	}


#ifndef _WIN32
	if(daemon)
	{
		size_t pid1;
		if( (pid1=fork())==0 )
		{
			setsid();
			if(fork()==0)
			{
				for (int i=getdtablesize();i>=0;--i) close(i);
				int i=open("/dev/null",O_RDWR);
				dup(i);
				dup(i);
			}
			else
			{
				exit(0);
			}
		}
		else
		{
			int status;
			waitpid(pid1, &status, 0);
			exit(0);
		}

		int rc = chdir((Server->getServerWorkingDir()).c_str());
		if (rc != 0)
		{
			Server->Log("Cannot set working directory to directory " + workingdir+" (2)", LL_ERROR);
		}
		
		if(pidfile.empty())
		{
			pidfile="/var/run/idrivebmr_srv.pid";
		}
		
		std::fstream pf;
		pf.open(pidfile.c_str(), std::ios::out|std::ios::binary);
		if(pf.is_open())
		{
			pf << getpid();
			pf.close();
		}
	}
#endif
	
	

	
	if(!logfile.empty())
	{
		g_logfile=logfile;
		g_logfile_user=daemon_user;
		Server->setLogFile(logfile, daemon_user);
	}	
	
	
	if(!loglevel.empty())
	{
		if(FileExists("debug_logging_enabled"))
			Server->setLogLevel(LL_DEBUG);
		else if(loglevel=="debug")
			Server->setLogLevel(LL_DEBUG);
		else if(loglevel=="warn")
			Server->setLogLevel(LL_WARNING);
		else if(loglevel=="info")
			Server->setLogLevel(LL_INFO);
		else if(loglevel=="error")
			Server->setLogLevel(LL_ERROR);
	}

	if(log_console_no_time)
	{
		Server->setLogConsoleTime(false);
	}

	if(is_big_endian())
	{
		Server->setLogLevel(LL_DEBUG);
	}

#ifndef _WIN32
	if( !daemon_user.empty() && (getuid()==0 || geteuid()==0) )
	{
		char buf[1000];
	    passwd pwbuf;
		passwd *pw;
		int rc=getpwnam_r(daemon_user.c_str(), &pwbuf, buf, 1000, &pw);
	    if(pw!=NULL)
	    {
			if (setgroups(0, NULL) != 0)
			{
				Server->Log("Unable to drop groups. Errno: " + convert(errno), LL_ERROR);
				return 44;
			}
			if (setgid(pw->pw_gid) != 0)
			{
				Server->Log("Unable to set group to gid "+convert(pw->pw_gid)+" of user \""+daemon_user+"\". Errno: " + convert(errno), LL_ERROR);
				return 44;
			}
			if (setuid(pw->pw_uid) != 0)
			{
				Server->Log("Unable to set user to uid " + convert(pw->pw_gid) + " of user \"" + daemon_user + "\". Errno: " + convert(errno), LL_ERROR);
				return 44;
			}
	    }
	    else
	    {
			Server->Log("Cannot get uid and gid of user \"" + daemon_user + "\" -1. Errno: " + convert(errno), LL_ERROR);
			return 44;
	    }
	}
	else if (!daemon_user.empty())
	{
		char buf[1000];
		passwd pwbuf;
		passwd *pw;
		int rc = getpwnam_r(daemon_user.c_str(), &pwbuf, buf, 1000, &pw);
		if (pw != NULL)
		{
			if (pw->pw_uid != getuid())
			{
				Server->Log("Running as wrong user. Specified user \"" + daemon_user + "\" with uid "+convert(pw->pw_uid)+" but running as uid "+convert(getuid()), LL_ERROR);
				return 44;
			}
		}
		else
		{
			Server->Log("Cannot get uid of user \""+daemon_user+"\". Errno: " + convert(errno), LL_ERROR);
			return 44;
		}
	}
#endif
	if( sqlite3_threadsafe()==0 )
	{
		Server->Log("SQLite3 wasn't compiled with the SQLITE_THREADSAFE. Exiting.", LL_ERROR);
		return 43;
	}

	sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
	//sqlite3_enable_shared_cache(1);

	{
		str_map::iterator iter=srv_params.find("sqlite_tmpdir");
		if(iter!=srv_params.end() && !iter->second.empty())
		{
			sqlite3_temp_directory=sqlite3_mprintf("%s", iter->second.c_str());
		}
	}


	for( size_t i=0;i<plugins.size();++i)
	{
		if( !Server->LoadDLL(plugins[i]) )
		{
			Server->Log("Loading "+(std::string)plugins[i]+" failed", LL_ERROR);
		}
	}

	Server->LoadStaticPlugins();
	
	CLoadbalancerClient *lbs=NULL;
	if( loadbalancer!="" )
	{
		lbs=new CLoadbalancerClient( loadbalancer, loadbalancer_port, loadbalancer_weight, port);
		
		Server->createThread(lbs);
	}

#ifndef _WIN32
	if (signal (SIGINT, termination_handler) == SIG_IGN)
		signal (SIGINT, SIG_IGN);
	/*if(!daemon)
	{
	    if (signal (SIGHUP, termination_handler) == SIG_IGN)
			signal (SIGHUP, SIG_IGN);
	}
	else*/
	{
	    if (signal (SIGHUP, hub_handler) == SIG_IGN)
			signal (SIGHUP, SIG_IGN);
	}
	if (signal (SIGTERM, termination_handler) == SIG_IGN)
		signal (SIGTERM, SIG_IGN);
#else
	signal(SIGABRT, abort_handler);
	_set_invalid_parameter_handler(invalid_parameter_handler);
#endif
	
	((CSessionMgr*)Server->getSessionMgr())->startTimeoutSessionThread();

	Server->startupComplete();

	if(no_server==false )
	{
		init_mutex_selthread();
		c_at=new CAcceptThread(workers, port);
		if(c_at->has_error())
		{
			Server->Log("Error while starting listening to ports. Stopping server.", LL_ERROR);
			run=false;
		}
#ifndef AS_SERVICE
		while(run==true)
		{
			(*c_at)(true);
	#ifdef _WIN32
			if( _kbhit()!=0 )
			{
				break;
			}
	#endif
		}

		Server->Log("Exited Loop");

		Server->Log("Deleting at..");
		delete c_at;
		
		destroy_mutex_selthread();
	}
	else
	{
		while(run==true)
		{
			Server->wait(1000);
		}
#endif
	}

#ifndef AS_SERVICE
	Server->Log("Deleting lbs...");
	delete lbs;
	Server->Log("Shutting down plugins...");
	Server->ShutdownPlugins();

	Server->Log("Deleting server...");
	delete Server;
	
	sqlite3_free(sqlite3_temp_directory);

#endif
	return 0;
}

#ifdef AS_SERVICE
void WINAPI my_service_main(DWORD argc, char_* argv[])
{
	if( c_at!=NULL )
	{
		(*c_at)(true);
	}
	else if(no_server==false)
	{
		Sleep(500);
		//throw std::exception("Service stopped");
	}
	else
	{
		Sleep(500);
	}
}
void my_shutdown_fcn(void)
{
	/*if(c_at !=NULL )
	{
		delete c_at;
		destroy_mutex_selthread();
		c_at=NULL;
	}
	if(Server!=NULL)
	{
		delete Server;
		Server=NULL;
	}*/
	if (Server != NULL)
	{
		Server->ShutdownPlugins();
	}
	nt_service&  service = nt_service::instance(L"IDriveBMRBackend");
	service.stop(0);
}
void my_stop_fcn(void)
{
	/*if(c_at !=NULL )
	{
		CAcceptThread *tmp=c_at;
		c_at=NULL;
		delete tmp;
		delete Server;
	}
	if(Server!=NULL)
	{
		CServer *srv=Server;
		Server=NULL;
		delete srv;
	}*/
	if(Server!=NULL)
	{
		Server->ShutdownPlugins();
	}
	nt_service&  service = nt_service::instance(L"IDriveBMRBackend");
	service.stop(0);
}

void my_init_fcn(void)
{
	my_init_fcn_t(srv_argc, srv_argv);
}

int main(int argc, char *argv[])
{
	SetCurrentDirectoryA(ExtractFilePath(argv[0]).c_str() );
	if( argc>1 && (std::string)argv[1]=="--cmdline" )
	{
		srv_argv=argv;
		srv_argc=argc;
	}
	else
	{
		std::string args=trim(getFile("args.txt"));

		for (size_t i = 0; i < 10; ++i)
		{
			std::string extra_args = trim(getFile("extra_args_" + convert(i) + ".txt"));
			if (!extra_args.empty())
			{
				args += "\n" + extra_args;
			}
		}

		int lc=linecount(args);
		srv_argv=new char*[lc+1];
		srv_argv[0]=new char[strlen(argv[0])+1];
		strcpy_s(srv_argv[0], strlen(argv[0])+1, argv[0]);
		for(int i=0;i<lc;++i)
		{
			std::string l=trim(getline(i, args));
			std::cout << l << std::endl;
			srv_argv[i+1]=new char[l.size()+1];
			memcpy(srv_argv[i+1], &l[0], l.size());
			srv_argv[i+1][l.size()]=0;
		}

		srv_argc=lc+1;
	}

	if( argc>1 && ( (std::string)argv[1]=="pgo" || (std::string)argv[1]=="cmdline" || (std::string)argv[1]=="--cmdline"  ) )
	{
		my_init_fcn();

		while(true)
		{
			my_service_main(0,NULL);
		}
	}
	else
	{
		// creates an access point to the instance of the service framework
		nt_service&  service = nt_service::instance(L"IDriveBMRBackend");

		// register "my_service_main" to be executed as the service main method 
		service.register_service_main( my_service_main );

		// register "my_init_fcn" as initialization fcn
		service.register_init_function( my_init_fcn );
		
		// config the service to accepts stop controls. Do nothing when it happens
		//service.accept_control( SERVICE_ACCEPT_STOP );
		service.register_control_handler( SERVICE_CONTROL_STOP, my_stop_fcn );

		// config the service to accepts shutdown controls and do something when receive it 
		service.register_control_handler( SERVICE_CONTROL_SHUTDOWN, my_shutdown_fcn );
			
		service.start();
	}
}
#endif
