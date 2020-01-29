#pragma once
#include "../Interface/Thread.h"
#include <memory>
#include <string>
#include "../Interface/Types.h"
#include "server_status.h"
#include "server_log.h"

class IDatabase;
class ServerSettings;
class ClientMain;
class ServerBackupDao;

struct SBackup
{
	SBackup()
		: incremental(0), incremental_ref(0), is_complete(false),
		  is_resumed(false), backupid(0), indexing_time_ms(0), backup_time_ms(0) {}

	int incremental;
	std::string path;
	int incremental_ref;
	std::string complete;
	bool is_complete;
	bool is_resumed;
	int backupid;
	int64 indexing_time_ms;
	int64 backup_time_ms;
};

enum LogAction
{
	LogAction_NoLogging,
	LogAction_LogIfNotDisabled,
	LogAction_AlwaysLog
};

class Backup : public IThread
{
public:
	Backup(ClientMain* client_main, int clientid, std::string clientname,
		std::string clientsubname, LogAction log_action, bool is_file_backup, bool is_incremental,
		std::string server_token, std::string details, bool scheduled);
	virtual ~Backup() {}

	virtual void operator()();

	bool getResult()
	{
		return backup_result;
	}

	bool isFileBackup()
	{
		return is_file_backup;
	}

	bool isIncrementalBackup()
	{
		return r_incremental;
	}

	bool shouldBackoff()
	{
		return should_backoff;
	}

	bool hasTimeoutError()
	{
		return has_timeout_error;
	}


	void setStopBackupRunning(bool b)
	{
		stop_backup_running = b;
	}

	logid_t getLogId()
	{
		return logid;
	}

	bool isScheduled()
	{
		return scheduled;
	}

protected:
	virtual bool doBackup() = 0;

	void setErrors(Backup& other);

	bool createDirectoryForClient();
	void saveClientLogdata(int image, int incremental, bool r_success, bool resumed);
	void sendLogdataMail(bool r_success, int image, int incremental, bool resumed, int errors, int warnings, int infos, std::string &data);
	std::string getUserRights(int userid, std::string domain);

	ClientMain* client_main;
	int clientid;
	std::string clientname;
	std::string clientsubname;
	LogAction log_action;

	IDatabase* db;
	std::auto_ptr<ServerSettings> server_settings;
	ServerBackupDao* backup_dao;

	bool log_backup;
	bool has_early_error;
	bool allow_remove_backup_folder;
	bool has_timeout_error;
	bool is_file_backup;
	bool r_resumed;
	bool r_incremental;
	bool should_backoff;
	size_t num_issues;
	bool scheduled;

	std::string details;

	bool backup_result;

	logid_t logid;
	size_t status_id;

	ActiveThread* active_thread;

	std::string server_token;

	bool stop_backup_running;
};