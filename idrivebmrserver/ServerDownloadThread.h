#pragma once

#include <string>
#include <deque>
#include <algorithm>
#include <assert.h>
#include <set>

#include "../Interface/Mutex.h"
#include "../Interface/Condition.h"
#include "../Interface/Pipe.h"
#include "../Interface/File.h"
#include "../Interface/Thread.h"
#include "../idrivebmrcommon/fileclient/FileClient.h"
#include "../idrivebmrcommon/fileclient/FileClientChunked.h"
#include "ClientMain.h"
#include "../idrivebmrcommon/file_metadata.h"


class FileClient;
class FileClientChunked;
class FilePathCorrections;
class MaxFileId;

namespace server {
	class FileMetadataDownloadThread;
}

namespace
{
	enum EFileClient
	{
		EFileClient_Full,
		EFileClient_Chunked,
	};

	enum EQueueAction
	{
		EQueueAction_Fileclient,
		EQueueAction_Quit,
		EQueueAction_StopShadowcopy,
		EQueueAction_StartShadowcopy,
		EQueueAction_Skip
	};

	struct SPatchDownloadFiles
	{
		bool prepared;
		bool prepare_error;
		IFile* orig_file;
		IFile* patchfile;
		IFile* chunkhashes;
		bool delete_chunkhashes;
		IFsFile* hashoutput;
		std::string hashpath;
		std::string filepath_old;
	};

	struct SQueueItem
	{
		SQueueItem()
			: id(std::string::npos),
			fileclient(EFileClient_Full),
			queued(false),
			action(EQueueAction_Fileclient),
			is_script(false),
			folder_items(0),
			script_end(false),
			switched(false),
			write_metadata(false)
		{
		}

		size_t id;
		std::string fn;
		std::string display_fn;
		std::string short_fn;
		std::string curr_path;
		std::string os_path;
		_i64 predicted_filesize;
		EFileClient fileclient;
		bool queued;
		EQueueAction action;
		SPatchDownloadFiles patch_dl_files;
		FileMetadata metadata;
		bool is_script;
        bool metadata_only;
		bool write_metadata;
		size_t folder_items;
		bool script_end;
		std::string sha_dig;
		unsigned int script_random;
		bool switched;
	};
	
	
	class IdRange
	{
	public:
		IdRange()
		 : min_id(std::string::npos), max_id(0), finalized(false)
		{}
		
		void add(size_t id)
		{
			max_id = (std::max)(id, max_id);
			min_id = (std::min)(id, min_id);
			ids.push_back(id);
			finalized=false;
		}
		
		void finalize()
		{
			std::sort(ids.begin(), ids.end());
			finalized=true;
		}
		
		bool hasId(size_t id)
		{
			assert(finalized);
			
			if(id>=min_id && id<=max_id)
			{
				return std::binary_search(ids.begin(), ids.end(), id);
			}
			else
			{
				return false;
			}
		}
		
	private:
		bool finalized;
		std::vector<size_t> ids;
		size_t min_id;
		size_t max_id;
	};
}



class ServerDownloadThread : public IThread, public FileClient::QueueCallback, public FileClientChunked::QueueCallback
{
public:
	ServerDownloadThread(FileClient& fc, FileClientChunked* fc_chunked, const std::string& backuppath, const std::string& backuppath_hashes, const std::string& last_backuppath, const std::string& last_backuppath_complete, bool hashed_transfer, bool save_incomplete_file, int clientid,
		const std::string& clientname, const std::string& clientsubname,
		bool use_tmpfiles, const std::string& tmpfile_path, const std::string& server_token, bool use_reflink, int backupid, bool r_incremental, IPipe* hashpipe_prepare, ClientMain* client_main,
		int filesrv_protocol_version, int incremental_num, logid_t logid, bool with_hashes, const std::vector<std::string>& shares_without_snapshot,
		bool with_sparse_hashing, server::FileMetadataDownloadThread* file_metadata_download, bool sc_failure_fatal, FilePathCorrections& filepath_corrections,
		MaxFileId& max_file_id);

	~ServerDownloadThread();

	void operator()(void);

	void addToQueueFull(size_t id, const std::string &fn, const std::string &short_fn, const std::string &curr_path, const std::string &os_path,
        _i64 predicted_filesize, const FileMetadata& metadata, bool is_script, bool metadata_only, size_t folder_items, const std::string& sha_dig,
		bool at_front_postpone_quitstop=false, unsigned int p_script_random=0, std::string display_fn = std::string(), bool write_metadata=false);

	void addToQueueChunked(size_t id, const std::string &fn, const std::string &short_fn, const std::string &curr_path,
		const std::string &os_path, _i64 predicted_filesize, const FileMetadata& metadata, bool is_script, const std::string& sha_dig, unsigned int p_script_random = 0, std::string display_fn=std::string());

	void addToQueueStartShadowcopy(const std::string& fn);

	void addToQueueStopShadowcopy(const std::string& fn);

	void queueStop();

	void queueSkip();

	void queueScriptEnd(const SQueueItem &todl);
	
	bool load_file(SQueueItem todl);
		
	bool load_file_patch(SQueueItem todl);

	bool logScriptOutput(std::string cfn, const SQueueItem &todl, std::string& sha_dig, int64 script_start_times, bool& hash_file);

	bool isDownloadOk(size_t id);

	bool isDownloadPartial(size_t id);
	
	bool isAllDownloadsOk();

	size_t getMaxOkId();

	bool isOffline();

	void hashFile(int64 fileid, std::string dstpath, std::string hashpath, IFile *fd, IFile *hashoutput, std::string old_file, int64 t_filesize,
		const FileMetadata& metadata, bool is_script, std::string sha_dig, IFile* sparse_extents_f, char hashing_method, bool has_snapshot);

	virtual bool getQueuedFileChunked(std::string& remotefn, IFile*& orig_file, IFile*& patchfile, IFile*& chunkhashes, IFsFile*& hashoutput, _i64& predicted_filesize, int64& file_id, bool& is_script);

	virtual void resetQueueFull();

	virtual std::string getQueuedFileFull(FileClient::MetadataQueue& metadata, size_t& folder_items, bool& finish_script, int64& file_id);

	virtual void unqueueFileFull(const std::string& fn, bool finish_script);

	virtual void unqueueFileChunked(const std::string& remotefn);

	virtual void resetQueueChunked();

	bool hasTimeout();

	bool shouldBackoff();

	bool sleepQueue();

	size_t getNumEmbeddedMetadataFiles();

	size_t getNumIssues();

	bool getHasDiskError();

	bool deleteTempFolder();

private:

	IFsFile* getTempFile();

	std::string getDLPath(const SQueueItem& todl);

	SPatchDownloadFiles preparePatchDownloadFiles(const SQueueItem& todl, bool& full_dl);

	bool start_shadowcopy(std::string path);

	bool stop_shadowcopy(std::string path);

	
	bool link_or_copy_file(const SQueueItem& todl);

	size_t insertFullQueueEarliest(const SQueueItem& ni, bool after_switched);
	bool hasFullQueuedAfter(std::deque<SQueueItem>::iterator it);

	void postponeQuitStop(size_t idx);

	bool fileHasSnapshot(const SQueueItem& todl);

	std::string tarFnToOsPath(const std::string& tar_path);

	void logVssLogdata();

	void base_dir_lost_hint();


	FileClient& fc;
	FileClientChunked* fc_chunked;
	const std::string& backuppath;
	const std::string& backuppath_hashes;
	const std::string& last_backuppath;
	const std::string& last_backuppath_complete;
	bool hashed_transfer;
	bool save_incomplete_file;
	int clientid;
	const std::string& clientname;
	const std::string& clientsubname;
	bool use_tmpfiles;
	const std::string& tmpfile_path;
	const std::string& server_token;
	bool use_reflink;
	int backupid;
	bool r_incremental;
	IPipe* hashpipe_prepare;
	ClientMain* client_main;
	int filesrv_protocol_version;
	bool skipping;
	int incremental_num;

	bool is_offline;
	bool has_timeout;
	bool exp_backoff;

	std::deque<SQueueItem> dl_queue;
	size_t queue_size;

	bool all_downloads_ok;
	IdRange download_nok_ids;
	IdRange download_partial_ids;
	size_t max_ok_id;

	bool with_metadata;
	bool with_hashes;
	
	size_t num_embedded_metadata_files;

	IMutex* mutex;
	ICondition* cond;

	logid_t logid;

	std::vector<std::string> shares_without_snapshot;

	bool with_sparse_hashing;
	char default_hashing_method;

	FilePathCorrections& filepath_corrections;
	std::map<std::string, std::set<std::string> > tar_filenames;

	server::FileMetadataDownloadThread* file_metadata_download;

	size_t num_issues;
	size_t last_snap_num_issues;

	bool has_disk_error;
	bool sc_failure_fatal;

	size_t tmpfile_num;

	MaxFileId& max_file_id;
};
