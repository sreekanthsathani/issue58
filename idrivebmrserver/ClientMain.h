#pragma once

#include "../Interface/Thread.h"
#include "../Interface/Database.h"
#include "../Interface/Query.h"
#include "../Interface/SettingsReader.h"
#include "../Interface/Mutex.h"
#include "../Interface/ThreadPool.h"
#include "../urlplugin/IUrlFactory.h"
#include "../idrivebmrcommon/fileclient/FileClient.h"
#include "../idrivebmrcommon/fileclient/FileClientChunked.h"
#include "../idrivebmrcommon/os_functions.h"
#include "server_hash.h"
#include "server_prepare_hash.h"
#include "server_status.h"
#include "../idrivebmrcommon/sha2/sha2.h"
#include "../idrivebmrcommon/fileclient/tcpstack.h"
#include "server_settings.h"
#include "../idrivebmrcommon/json.h"

#include <memory>
#include "server_log.h"

class ServerVHDWriter;
class IFile;
class IPipe;
class ServerPingThread;
class FileClient;
class IPipeThrottler;
class BackupServerContinuous;
class ContinuousBackup;
class Backup;
class ServerBackupDao;
class ImageBackup;
class ServerChannelThread;

const int c_group_default = 0;
const int c_group_continuous = 1;
const int c_group_max = 99;
const int c_group_size = 100;

struct SProtocolVersions
{
	SProtocolVersions() :
			filesrv_protocol_version(0), file_protocol_version(0),
				file_protocol_version_v2(0), set_settings_version(0),
				image_protocol_version(0), eta_version(0), cdp_version(0),
				efi_version(0), file_meta(0), select_sha_version(0),
				client_bitmap_version(0), cmd_version(0),
				symbit_version(0), phash_version(0),
				wtokens_version(0)
			{

			}

	int filesrv_protocol_version;
	int file_protocol_version;
	int file_protocol_version_v2;
	int set_settings_version;
	int image_protocol_version;
	int eta_version;
	int cdp_version;
	int efi_version;
	int file_meta;
	int select_sha_version;
	int client_bitmap_version;
	int cmd_version;
	int require_previous_cbitmap;
	int async_index_version;
	int symbit_version;
	int phash_version;
	int wtokens_version;
	std::string os_simple;
};

struct SRunningBackup
{
	SRunningBackup()
		: backup(NULL), ticket(ILLEGAL_THREADPOOL_TICKET),
		group(c_group_default)
	{

	}


	Backup* backup;
	THREADPOOL_TICKET ticket;
	int group;
	std::string letter;
};

struct SRunningRestore
{
	SRunningRestore(std::string restore_identity, logid_t log_id, int64 status_id, int64 restore_id)
		: restore_identity(restore_identity), log_id(log_id), status_id(status_id), restore_id(restore_id),
		  last_active(Server->getTimeMS())
	{}

	std::string restore_identity;
	logid_t log_id;
	int64 status_id;
	int64 restore_id;
	int64 last_active;
};

struct SShareCleanup
{
	SShareCleanup(std::string name, std::string identity, bool cleanup_file, bool remove_callback)
		: name(name), identity(identity), cleanup_file(cleanup_file), remove_callback(remove_callback)
	{

	}

	std::string name;
	std::string identity;
	bool cleanup_file;
	bool remove_callback;
};

class ClientMain : public IThread, public FileClientChunked::ReconnectionCallback,
	public FileClient::ReconnectionCallback, public INotEnoughSpaceCallback,
	public FileClient::NoFreeSpaceCallback, public FileClientChunked::NoFreeSpaceCallback,
	public FileClient::ProgressLogCallback
{
public:
	ClientMain(IPipe *pPipe, sockaddr_in pAddr, const std::string &pName, const std::string& pSubName, const std::string& pMainName, int filebackup_group_offset,
		bool internet_connection, bool use_file_snapshots, bool use_image_snapshots, bool use_reflink);
	~ClientMain(void);

	void operator()(void);

	bool authenticateIfNeeded(bool retry_exit, bool force);

	struct SConnection
	{
		SConnection()
			: conn(),
			internet_connection(false)
		{}

		std::auto_ptr<IPipe> conn;
		bool internet_connection;
	};

	bool sendClientMessage(const std::string &msg, const std::string &retok, const std::string &errmsg, unsigned int timeout, bool logerr=true, int max_loglevel=LL_ERROR, bool *retok_err=NULL, std::string* retok_str=NULL, SConnection* conn=NULL);
	bool sendClientMessageRetry(const std::string &msg, const std::string &retok, const std::string &errmsg, unsigned int timeout, size_t retry=0, bool logerr=true, int max_loglevel=LL_ERROR, bool *retok_err=NULL, std::string* retok_str=NULL);
	std::string sendClientMessage(const std::string &msg, const std::string &errmsg, unsigned int timeout, bool logerr=true, int max_loglevel=LL_ERROR, SConnection* conn=NULL);
	std::string sendClientMessageRetry(const std::string &msg, const std::string &errmsg, unsigned int timeout, size_t retry=0, bool logerr=true, int max_loglevel=LL_ERROR, unsigned int timeout_after_first=0);
	void sendToPipe(const std::string &msg);

	bool createDirectoryForClient();

	sockaddr_in getClientaddr(void);

	static void init_mutex(void);
	static void destroy_mutex(void);

	static MailServer getMailServerSettings(void);
	static bool sendMailToAdmins(const std::string& subj, const std::string& message);

	static int getNumberOfRunningBackups(void);
	static int getNumberOfRunningFileBackups(void);
	static int getClientID(IDatabase *db, const std::string &clientname, ServerSettings *server_settings, bool *new_client, std::string* authkey=NULL);

	IPipe *getClientCommandConnection(ServerSettings* server_settings, int timeoutms=10000, std::string* clientaddr=NULL);

	virtual IPipe * new_fileclient_connection(void);

	virtual bool handle_not_enough_space(const std::string &path);

	static IFsFile *getTemporaryFileRetry(bool use_tmpfiles, const std::string& tmpfile_path, logid_t logid);

	static void destroyTemporaryFile(IFile *tmp);

	bool UpdateCloudVirtualization(int backupId);
	bool PreviousVirtualizeVerificationComplete();
	
	
	virtual void log_progress( const std::string& fn, int64 total, int64 downloaded, int64 speed_bps );

	_u32 getClientFilesrvConnection(FileClient *fc, ServerSettings* server_settings, int timeoutms=10000);

	bool getClientChunkedFilesrvConnection(std::auto_ptr<FileClientChunked>& fc_chunked, ServerSettings* server_settings, int timeoutms=10000);

	bool isOnInternetConnection()
	{
		return internet_connection;
	}

	SProtocolVersions getProtocolVersions()
	{
		return protocol_versions;
	}

	void refreshSessionIdentity()
	{
		IScopedLock lock(clientaddr_mutex);
		session_identity_refreshtime = Server->getTimeMS();
	}

	std::string getIdentity();
	
	int getCurrImageVersion()
	{
		return curr_image_version;
	}

	static bool run_script(std::string name, const std::string& params, logid_t logid);

	void stopBackupRunning(bool file);

	void updateClientAddress(const std::string& address_data);

	IPipe* getInternalCommandPipe()
	{
		return pipe;
	}

	void setContinuousBackup(BackupServerContinuous* cb);

	void addContinuousChanges( const std::string& changes );

	static void addShareToCleanup(int clientid, const SShareCleanup& cleanupData);

	static void cleanupRestoreShare(int clientid, std::string restore_identity);

	bool finishRestore(int64 restore_id);

	bool updateRestoreRunning(int64 restore_id);

	static bool startBackupBarrier(int64 timeout_seconds);

	static void stopBackupBarrier();

	void updateCapa();

	std::vector<SLogEntry> parseLogData(int64 max_duration, const std::string& data);

	bool isDataplanOkay(ServerSettings* local_settings, bool file);

	void setConnectionMetered(bool b);

	void forceReauthenticate();

	static std::string normalizeVolumeUpper(std::string volume);


private:
	void unloadSQL(void);
	void prepareSQL(void);
	void updateLastseen(int64 lastseen);
	bool isUpdateFull(int tgroup);
	bool isUpdateIncr(int tgroup);
	bool isUpdateFullImage(void);
	bool isUpdateIncrImage(void);
	bool isUpdateFullImage(const std::string &letter);
	bool isUpdateIncrImage(const std::string &letter);
	void sendClientBackupIncrIntervall(void);
	void sendSettings(void);
	bool getClientSettings(bool& doesnt_exist);
	bool updateClientSetting(const std::string &key, const std::string &value);
	void sendClientLogdata(void);
	bool isRunningImageBackup(const std::string& letter);
	bool isImageGroupQueued(const std::string& letter, bool full);
	bool isRunningFileBackup(int group, bool queue_only=true);	
	void checkClientVersion(void);
	bool sendFile(IPipe *cc, IFile *f, int timeout);
	bool isBackupsRunningOkay(bool file, bool incr=false);	
	bool updateCapabilities(void);
	IPipeThrottler *getThrottler(int speed_bps);
	bool inBackupWindow(Backup* backup);
	void updateClientAccessKey();
	bool isDataplanOkay(bool file);
	bool isOnline(ServerChannelThread& channel_thread);

	unsigned int exponentialBackoffTime(size_t count, unsigned int sleeptime, unsigned div);
	bool exponentialBackoff(size_t count, int64 lasttime, unsigned int sleeptime, unsigned div);
	unsigned int exponentialBackoffTimeImage();
	unsigned int exponentialBackoffTimeFile();
	bool exponentialBackoffImage();
	bool exponentialBackoffFile();
	bool exponentialBackoffCdp();
	bool pauseRetryBackup();

	bool sendServerIdentity(bool retry_exit);
	bool authenticatePubKey();

	void timeoutRestores();

	void cleanupShares();
	static void cleanupShare(SShareCleanup& tocleanup);

	void finishFailedRestore(std::string restore_identity, logid_t log_id, int64 status_id, int64 restore_id);
	//bool JsonizeRetrievedData(std::vector<int> backupInfo, std::string &jsonData);
	bool JsonizeRetrievedData(std::vector<int> backupIds, JSON::Object &recoveryId);
	bool InvokePostBackupScripts(std::vector<int> backupInfo);
	bool GetIntegrityStatus(std::map<std::string, std::string> drivePath);
	bool ValidateVirtualization(std::vector<int> backupIds, JSON::Object &virtObject);
	void GetClientLogID(void);

	std::string curr_image_format;

	IPipe *pipe;
	IDatabase *db;

	sockaddr_in clientaddr;
	IMutex *clientaddr_mutex;
	std::string clientname;
	std::string clientsubname;
	std::string clientmainname;
	int filebackup_group_offset;

	std::string tmpfile_path;
	static size_t tmpfile_num;
	static IMutex *tmpfile_mutex;

	int clientid;

	ServerSettings *server_settings;

	IQuery *q_update_lastseen;
	IQuery *q_update_setting;
	IQuery *q_get_setting;
	IQuery *q_insert_setting;
	IQuery *q_get_unsent_logdata;
	IQuery *q_set_logdata_sent;

	bool can_backup_images;

	bool do_full_backup_now;
	bool do_incr_backup_now;
	bool do_update_settings;
	bool do_full_image_now;
	bool do_incr_image_now;
	bool do_update_access_key;
	bool cdp_needs_sync;

	static int running_backups;
	static int running_file_backups;
	static IMutex *running_backup_mutex;

	SProtocolVersions protocol_versions;
	volatile bool internet_connection;
	volatile bool connection_metered;
	int update_version;
	std::string all_volumes;
	std::string all_nonusb_volumes;

	bool use_file_snapshots;
	bool use_image_snapshots;
	bool use_reflink;
	bool use_tmpfiles;
	bool use_tmpfiles_images;

	CTCPStack tcpstack;

	IMutex* throttle_mutex;
	IPipeThrottler *client_throttler;

	int64 last_backup_try;
	
	int64 last_image_backup_try;
	size_t count_image_backup_try;

	int64 last_file_backup_try;
	size_t count_file_backup_try;

	int64 last_cdp_backup_try;
	size_t count_cdp_backup_try;

	std::string session_identity;
	int64 session_identity_refreshtime;

	ServerBackupDao* backup_dao;

	int64 client_updated_time;
	
	BackupServerContinuous* continuous_backup;
	IMutex* continuous_mutex;

	unsigned int curr_image_version;

	std::vector<SRunningBackup> backup_queue;

	static IMutex* cleanup_mutex;
	static std::map<int, std::vector<SShareCleanup> > cleanup_shares;

	logid_t logid;
	static int restore_client_id;
	static bool running_backups_allowed;

	int last_incr_freq;
	int last_startup_backup_delay;

	std::string curr_server_token;
	bool needs_authentification;

	std::auto_ptr<IMutex> restore_mutex;
	std::vector<SRunningRestore> running_restores;

	std::vector< std::map<ImageBackup*, bool>  > running_image_groups;

	volatile bool update_capa;

	volatile bool do_reauthenticate;

	std::vector<int> allBackupsInDatabase;
	std::vector<std::string> virtualizationLogid;
};
