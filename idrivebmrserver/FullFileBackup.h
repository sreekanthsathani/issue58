#pragma once

#include "FileBackup.h"

class FullFileBackup : public FileBackup
{
public:
	FullFileBackup(ClientMain* client_main, int clientid, std::string clientname, std::string clientsubname,
		LogAction log_action, int group, bool use_tmpfiles, std::string tmpfile_path, bool use_reflink,
		bool use_snapshots, std::string server_token, std::string details, bool scheduled);

protected:
	virtual bool doFileBackup();


	SBackup getLastFullDurations();
};