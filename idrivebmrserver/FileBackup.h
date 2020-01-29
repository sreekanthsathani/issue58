#pragma once
#include "Backup.h"
#include "../Interface/Types.h"
#include "dao/ServerBackupDao.h"
#include <string>
#include "server_settings.h"
#include "../idrivebmrcommon/fileclient/FileClient.h"
#include "../idrivebmrcommon/fileclient/FileClientChunked.h"
#include <map>
#include "../idrivebmrcommon/file_metadata.h"
#include "server_log.h"
#include "FileMetadataDownloadThread.h"
#include <set>

class ClientMain;
class BackupServerHash;
class BackupServerPrepareHash;
class ServerPingThread;
class FileIndex;
class PhashLoad;

namespace
{
	const unsigned int status_update_intervall=1000;
	const unsigned int eta_update_intervall=60000;
	const char* sync_fn = ".sync_f3a50226-f49a-4195-afef-c75b21781ae1";
}

struct SContinuousSequence
{
	SContinuousSequence()
		: id(-1), next(-1)
	{

	}

	SContinuousSequence(int64 id, int64 next)
		: id(id), next(next)
	{

	}
	int64 id;
	int64 next;
};

class FilePathCorrections
{
public:
	FilePathCorrections()
		: mutex(Server->createMutex()) {

	}
	void add(const std::string& path, const std::string& corr) {
		IScopedLock lock(mutex.get());
		filepath_corrections.insert(std::make_pair(path, corr));
	}
	bool get(const std::string& path, std::string& corr) {
		IScopedLock lock(mutex.get());
		std::map<std::string, std::string>::iterator it = filepath_corrections.find(path);
		if (it != filepath_corrections.end())
		{
			corr = it->second;
			return true;
		}
		return false;
	}
private:
	std::auto_ptr<IMutex> mutex;
	std::map<std::string, std::string> filepath_corrections;
};

class MaxFileId
{
public:
	MaxFileId()
		: mutex(Server->createMutex()),
		max_downloaded(std::string::npos), max_preprocessed(0),
		min_downloaded(0)
	{}

	void setMinDownloaded(size_t id)
	{
		IScopedLock lock(mutex.get());
		if (max_downloaded>id)
		{
			max_downloaded = id;
		}
		min_downloaded = id+1;
	}

	void setMaxDownloaded(size_t id)
	{
		IScopedLock lock(mutex.get());
		max_downloaded = id + 1;
		if (max_downloaded >= min_downloaded)
		{
			max_downloaded = std::string::npos;
		}
	}

	void setMaxPreProcessed(size_t id)
	{
		IScopedLock lock(mutex.get());
		max_preprocessed = id + 1;
	}

	bool isFinished(size_t id)
	{
		IScopedLock lock(mutex.get());
		if (id + 1 <= max_downloaded
			&& id + 1 <= max_preprocessed)
		{
			return true;
		}
		return false;
	}

private:
	std::auto_ptr<IMutex> mutex;
	size_t max_downloaded;
	size_t max_preprocessed;
	size_t min_downloaded;
};

class FileBackup : public Backup, public FileClient::ProgressLogCallback
{
public:
	FileBackup(ClientMain* client_main, int clientid, std::string clientname, std::string subclientname, LogAction log_action,
		bool is_incremental, int group, bool use_tmpfiles, std::string tmpfile_path, bool use_reflink, bool use_snapshots,
		std::string server_token, std::string details, bool scheduled);
	~FileBackup();

	bool getResult();
	bool hasEarlyError();
	bool hasDiskError();

	int getBackupid()
	{
		return backupid;
	}

	std::map<std::string, SContinuousSequence> getContinuousSequences()
	{
		return continuous_sequences;
	}

	static std::string convertToOSPathFromFileClient(std::string path);

	static std::string fixFilenameForOS(std::string fn, std::set<std::string>& samedir_filenames, const std::string& curr_path, bool log_warnings, logid_t logid, FilePathCorrections& filepath_corrections);

	virtual void log_progress(const std::string& fn, int64 total, int64 downloaded, int64 speed_bps);

	static bool create_hardlink(const std::string &linkname, const std::string &fname, bool use_ioref, bool* too_many_links);

protected:
	virtual bool doBackup();

	virtual bool doFileBackup() = 0;

	ServerBackupDao::SDuration interpolateDurations(const std::vector<ServerBackupDao::SDuration>& durations);
	bool request_filelist_construct(bool full, bool resume, int group,
		bool with_token, bool& no_backup_dirs, bool& connect_fail, const std::string& clientsubname);
	bool wait_for_async(const std::string& async_id, int64 timeout_time=10*60*1000);
	bool request_client_write_tokens();
	void logVssLogdata(int64 vss_duration_s);
	bool getTokenFile(FileClient &fc, bool hashed_transfer, bool request);
	std::string clientlistName(int ref_backupid);
	void createHashThreads(bool use_reflink, bool ignore_hash_mismatches);
	void destroyHashThreads();
	_i64 getIncrementalSize(IFile *f, const std::vector<size_t> &diffs, bool& backup_with_components, bool all=false);
	void calculateDownloadSpeed(int64 ctime, FileClient &fc, FileClientChunked* fc_chunked);
	void calculateEtaFileBackup( int64 &last_eta_update, int64& eta_set_time, int64 ctime, FileClient &fc, FileClientChunked* fc_chunked,
		int64 linked_bytes, int64 &last_eta_received_bytes, double &eta_estimated_speed, _i64 files_size );
	bool hasChange(size_t line, const std::vector<size_t> &diffs);
	bool link_file(const std::string &fn, const std::string &short_fn, const std::string &curr_path,
		const std::string &os_path, const std::string& sha2, _i64 filesize, bool add_sql, FileMetadata& metadata);
	void sendBackupOkay(bool b_okay);
	void notifyClientBackupSuccessful(void);
	void waitForFileThreads();
	bool verify_file_backup(IFile *fileentries);
	void save_debug_data(const std::string& rfn, const std::string& local_hash, const std::string& remote_hash);
	std::string getSHA256(const std::string& fn);
	std::string getSHA512(const std::string& fn);
	std::string getSHADef(const std::string& fn);
	bool constructBackupPath(bool on_snapshot, bool create_fs, std::string& errmsg);
	bool constructBackupPathCdp();
	std::string systemErrorInfo();
	void saveUsersOnClient();
	void createUserViews(IFile* file_list_f);
	bool createUserView(IFile* file_list_f, const std::vector<int64>& ids, std::string accoutname, const std::vector<size_t>& identical_permission_roots);
	std::vector<size_t> findIdenticalPermissionRoots(IFile* file_list_f, const std::vector<int64>& ids);
	void deleteBackup();
	bool createSymlink(const std::string& name, size_t depth, const std::string& symlink_target, const std::string& dir_sep, bool isdir);
	bool startFileMetadataDownloadThread();
	bool stopFileMetadataDownloadThread(bool stopped, size_t expected_embedded_metadata_files);
	void parseSnapshotFailed(const std::string& logline);
	std::string permissionsAllowAll();
	bool loadWindowsBackupComponentConfigXml(FileClient &fc);
	bool startPhashDownloadThread(const std::string& async_id);
	bool stopPhashDownloadThread();

	int group;
	bool use_tmpfiles;
	std::string tmpfile_path;
	bool use_reflink;
	bool use_snapshots;
	bool with_hashes;
	bool cdp_path;

	std::string backuppath;
	std::string dir_pool_path;
	std::string backuppath_hashes;
	std::string backuppath_single;

	IPipe *hashpipe;
	IPipe *hashpipe_prepare;
	BackupServerHash *bsh;
	THREADPOOL_TICKET bsh_ticket;
	BackupServerPrepareHash *bsh_prepare;
	THREADPOOL_TICKET bsh_prepare_ticket;
	std::auto_ptr<BackupServerHash> local_hash;

	ServerPingThread* pingthread;
	THREADPOOL_TICKET pingthread_ticket;

	std::map<std::string, SContinuousSequence> continuous_sequences;

	std::auto_ptr<FileIndex> fileindex;

	bool disk_error;

	int backupid;

    std::auto_ptr<server::FileMetadataDownloadThread> metadata_download_thread;
	THREADPOOL_TICKET metadata_download_thread_ticket;
	std::auto_ptr<server::FileMetadataDownloadThread::FileMetadataApplyThread> metadata_apply_thread;
	THREADPOOL_TICKET metadata_apply_thread_ticket;

	FilePathCorrections filepath_corrections;

	std::vector<std::string> shares_without_snapshot;

	int64 last_speed_received_bytes;
	int64 speed_set_time;

	std::auto_ptr<PhashLoad> phash_load;
	THREADPOOL_TICKET phash_load_ticket;

	MaxFileId max_file_id;
};
