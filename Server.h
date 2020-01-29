#pragma once
#define NO_INTERFACE

#ifdef _WIN32
#	include <thread>
#else
#	include <pthread.h>
#endif

#include "Interface/Server.h"
#include "Interface/Action.h"
#include "Interface/Mutex.h"
#include "Interface/Condition.h"
#include "Interface/SharedMutex.h"
#include <vector>
#include <fstream>
#include <memory>

typedef void(*LOADACTIONS)(IServer*);
typedef void(*UNLOADACTIONS)(void);

#ifdef _WIN32
#include <windows.h>
#else
typedef void *HMODULE;
#endif

class FCGIRequest;
class IDatabaseInt;
class IDatabaseFactory;
class CSessionMgr;
class CServiceAcceptor;
class CThreadPool;
class IOutputStream;

struct SDatabase
{
	SDatabase(IDatabaseFactory *factory, const std::string &file)
		: factory(factory), file(file), allocation_chunk_size(std::string::npos)
	{}

	IDatabaseFactory *factory;
	std::string file;
	std::map<THREAD_ID, IDatabaseInt*> tmap;
	std::vector<std::pair<std::string,std::string> > attach;
	size_t allocation_chunk_size;
	std::auto_ptr<ISharedMutex> single_user_mutex;
	std::auto_ptr<IMutex> lock_mutex;
	std::auto_ptr<int> lock_count;
	std::auto_ptr<ICondition> unlock_cond;
	str_map params;

private:
	SDatabase(const SDatabase& other) {}
	void operator=(const SDatabase& other){}
};


class CServer : public IServer
{
public:
	CServer();
	~CServer();
	void setup(void);
	void setServerParameters(const str_map &pServerParams);

	virtual std::string getServerParameter(const std::string &key);
	virtual std::string getServerParameter(const std::string &key, const std::string &def);
	virtual void setServerParameter(const std::string &key, const std::string &value);
	virtual void setLogLevel(int LogLevel);
	virtual void setLogFile(const std::string &plf, std::string chown_user="");
	virtual void setLogCircularBufferSize(size_t size);
	virtual std::vector<SCircularLogEntry> getCicularLogBuffer(size_t minid);
	virtual void Log(const std::string &pStr, int LogLevel=LL_INFO);
	virtual bool Write(THREAD_ID tid, const std::string &str, bool cached=true);
	virtual bool WriteRaw(THREAD_ID tid, const char *buf, size_t bsize, bool cached=true);

	virtual void setContentType(THREAD_ID tid, const std::string &str);
	virtual void addHeader(THREAD_ID tid, const std::string &str);

	THREAD_ID Execute(const std::string &action, const std::string &context, str_map &GET, str_map &POST, str_map &PARAMS, IOutputStream *req);
	std::string Execute(const std::string &action, const std::string &context, str_map &GET, str_map &POST, str_map &PARAMS);

	virtual void AddAction(IAction *action);
	virtual bool RemoveAction(IAction *action);
	virtual void setActionContext(std::string context);
	virtual void resetActionContext(void);

	virtual int64 getTimeSeconds(void);
	virtual int64 getTimeMS(void);

	virtual bool LoadDLL(const std::string &name);
	virtual bool UnloadDLL(const std::string &name);
	void LoadStaticPlugins();

	virtual void destroy(IObject *obj);

	virtual void wait(unsigned int ms);

	virtual ITemplate* createTemplate(std::string pFile);
	virtual IMutex* createMutex(void);
	virtual ISharedMutex* createSharedMutex();
	virtual ICondition* createCondition(void);
	virtual IPipe *createMemoryPipe(void);
	virtual bool createThread(IThread *thread, const std::string& name = std::string(), CreateThreadFlags flags = CreateThreadFlags_None);
	virtual void setCurrentThreadName(const std::string& name);
	virtual IThreadPool *getThreadPool(void);
	virtual ISettingsReader* createFileSettingsReader(const std::string& pFile);
	virtual ISettingsReader* createDBSettingsReader(THREAD_ID tid, DATABASE_ID pIdentifier, const std::string &pTable, const std::string &pSQL="");
	virtual ISettingsReader* createDBSettingsReader(IDatabase *db, const std::string &pTable, const std::string &pSQL="");
	virtual ISettingsReader* createDBMemSettingsReader(THREAD_ID tid, DATABASE_ID pIdentifier, const std::string &pTable, const std::string &pSQL = "");
	virtual ISettingsReader* createDBMemSettingsReader(IDatabase *db, const std::string &pTable, const std::string &pSQL = "");
	virtual ISettingsReader* createMemorySettingsReader(const std::string &pData);
	virtual IPipeThrottler* createPipeThrottler(size_t bps, bool percent_max);
	virtual IPipeThrottler* createPipeThrottler(IPipeThrottlerUpdater* updater);
	virtual IThreadPool* createThreadPool(size_t max_threads, size_t max_waiting_threads, const std::string& idle_name);

	virtual bool openDatabase(std::string pFile, DATABASE_ID pIdentifier, const str_map& params = str_map(), std::string pEngine="sqlite");
	virtual IDatabase* getDatabase(THREAD_ID tid, DATABASE_ID pIdentifier);
	virtual void destroyAllDatabases(void);
	virtual void destroyDatabases(THREAD_ID tid);

	virtual ISessionMgr *getSessionMgr(void);
	virtual IPlugin* getPlugin(THREAD_ID tid, PLUGIN_ID pIdentifier);

	virtual THREAD_ID getThreadID(void);
	
	virtual std::string ConvertToUTF16(const std::string &input);
	virtual std::string ConvertToUTF32(const std::string &input);
	virtual std::wstring ConvertToWchar(const std::string &input);
	virtual std::string ConvertFromWchar(const std::wstring &input);
	virtual std::string ConvertFromUTF16(const std::string &input);
	virtual std::string ConvertFromUTF32(const std::string &input);

	virtual std::string GenerateHexMD5(const std::string &input);
	virtual std::string GenerateBinaryMD5(const std::string &input);

	virtual void StartCustomStreamService(IService *pService, std::string pServiceName, unsigned short pPort, int pMaxClientsPerThread=-1, IServer::BindTarget bindTarget=IServer::BindTarget_All);
	virtual IPipe* ConnectStream(std::string pServer, unsigned short pPort, unsigned int pTimeoutms);
	virtual IPipe *PipeFromSocket(SOCKET pSocket);
	virtual void DisconnectStream(IPipe *pipe);
	virtual std::string LookupHostname(const std::string& pIp);

	virtual bool RegisterPluginPerThreadModel(IPluginMgr *pPluginMgr, std::string pName);
	virtual bool RegisterPluginThreadsafeModel(IPluginMgr *pPluginMgr, std::string pName);

	virtual PLUGIN_ID StartPlugin(std::string pName, str_map &params);

	virtual bool RestartPlugin(PLUGIN_ID pIdentifier);

	virtual unsigned int getNumRequests(void);
	virtual void addRequest(void);

	virtual IFsFile* openFile(std::string pFilename, int pMode=0);
	virtual IFsFile* openFileFromHandle(void *handle, const std::string& pFilename);
	virtual IFsFile* openTemporaryFile(void);
	virtual IFile* openMemoryFile(void);
	virtual bool deleteFile(std::string pFilename);
	virtual bool fileExists(std::string pFilename);

	virtual POSTFILE_KEY getPostFileKey();
	virtual void addPostFile(POSTFILE_KEY pfkey, const std::string &name, const SPostfile &pf);
	virtual SPostfile getPostFile(POSTFILE_KEY pfkey, const std::string &name);
	virtual void clearPostFiles(POSTFILE_KEY pfkey);

	virtual std::string getServerWorkingDir(void);
	void setServerWorkingDir(const std::string &wdir);

	void ShutdownPlugins(void);

	void setTemporaryDirectory(const std::string &dir);

	virtual void registerDatabaseFactory(const std::string &pEngineName, IDatabaseFactory *factory);
	virtual bool hasDatabaseFactory(const std::string &pEngineName);

	virtual bool attachToDatabase(const std::string &pFile, const std::string &pName, DATABASE_ID pIdentifier);
	virtual bool setDatabaseAllocationChunkSize(DATABASE_ID pIdentifier, size_t allocation_chunk_size);

	static int WriteDump(void* pExceptionPointers);

	virtual void waitForStartupComplete(void);
	void startupComplete(void);

	void shutdown(void);

	void initRandom(unsigned int seed);
	virtual unsigned int getRandomNumber(void);
	virtual std::vector<unsigned int> getRandomNumbers(size_t n);
	virtual void randomFill(char *buf, size_t blen);

	virtual unsigned int getSecureRandomNumber(void);
	virtual std::vector<unsigned int> getSecureRandomNumbers(size_t n);
	virtual void secureRandomFill(char *buf, size_t blen);
	virtual std::string secureRandomString(size_t len);

	virtual void setFailBit(size_t failbit);
	virtual void clearFailBit(size_t failbit);
	virtual size_t getFailBits(void);

	virtual void clearDatabases(THREAD_ID tid);

	void setLogRotationFilesize(size_t filesize);

	void setLogRotationNumFiles(size_t numfiles);

	void setLogConsoleTime(bool b);

private:

	void logToCircularBuffer(const std::string& msg, int loglevel);

	bool UnloadDLLs(void);
	void UnloadDLLs2(void);

	void rotateLogfile();


	int loglevel;
	bool logfile_a;
	std::fstream logfile;

	IMutex* log_mutex;
	IMutex* action_mutex;
	IMutex* requests_mutex;
	IMutex* outputs_mutex;
	IMutex* db_mutex;
	IMutex* thread_mutex;
	IMutex* plugin_mutex;
	IMutex* rps_mutex;
	IMutex* postfiles_mutex;
	IMutex* param_mutex;
	IMutex* startup_complete_mutex;
	IMutex* rnd_mutex;

	ICondition *startup_complete_cond;
	bool startup_complete;

	std::map< std::string, std::map<std::string, IAction*> > actions;

	std::map<std::string, UNLOADACTIONS> unload_functs;
	std::vector<HMODULE> unload_handles;

	std::map<THREAD_ID, IOutputStream*> current_requests;
	std::map<THREAD_ID, std::pair<bool, std::string> > current_outputs;

	THREAD_ID curr_thread_id;
#ifdef _WIN32
	std::map<std::thread::id, THREAD_ID> threads;
#else
	std::map<pthread_t, THREAD_ID> threads;
#endif

	std::map<DATABASE_ID, SDatabase*> databases;

	CSessionMgr *sessmgr;

	std::vector<CServiceAcceptor*> stream_services;

	std::map<PLUGIN_ID, std::map<THREAD_ID, IPlugin*> > perthread_plugins;
	std::map<std::string, IPluginMgr*> perthread_pluginmgrs;
	std::map<PLUGIN_ID, std::pair<IPluginMgr*,str_map> > perthread_pluginparams;

	std::map<std::string, IPluginMgr*> threadsafe_pluginmgrs;
	std::map<PLUGIN_ID, IPlugin*> threadsafe_plugins;

	std::map<POSTFILE_KEY, std::map<std::string, SPostfile > > postfiles;
	POSTFILE_KEY curr_postfilekey;

	str_map server_params;

	PLUGIN_ID curr_pluginid;

	unsigned int num_requests;

	CThreadPool* threadpool;

	std::string action_context;

	std::string workingdir;

	std::string tmpdir;

	std::map<std::string, IDatabaseFactory*> database_factories;

	size_t circular_log_buffer_id;

	std::vector<SCircularLogEntry> circular_log_buffer;
	size_t circular_log_buffer_idx;

	bool has_circular_log_buffer;

	size_t failbits;

	std::string logfile_fn;

	std::string logfile_chown_user;

	size_t log_rotation_size;

	bool log_console_time;

	size_t log_rotation_files;
};

#ifndef DEF_SERVER
extern CServer *Server;
#endif
