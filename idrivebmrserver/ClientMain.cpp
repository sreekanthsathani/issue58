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
#include "ClientMain.h"
#include "server_ping.h"
#include "database.h"
#include "../stringtools.h"
#include "../idrivebmrcommon/fileclient/FileClient.h"
#include "../Interface/Server.h"
#include "../Interface/ThreadPool.h"
#include "../idrivebmrcommon/fileclient/tcpstack.h"
#include "../common/data.h"
#include "../idrivebmrcommon/settingslist.h"
#include "server_channel.h"
#include "server_log.h"
#include "ServerDownloadThread.h"
#include "InternetServiceConnector.h"
#include "server_update_stats.h"
#include "../idrivebmrcommon/escape.h"
#include "../common/adler32.h"
#include "server_running.h"
#include "server_cleanup.h"
#include "treediff/TreeDiff.h"
#include "../urlplugin/IUrlFactory.h"
#include "../idrivebmrcommon/mbrdata.h"
#include "../Interface/PipeThrottler.h"
#include "snapshot_helper.h"
#include "../cryptoplugin/ICryptoFactory.h"
#include "server_dir_links.h"
#include "server.h"
#include "../idrivebmrcommon/filelist_utils.h"
#include "server_continuous.h"
#include <algorithm>
#include <memory.h>
#include <time.h>
#include <stdio.h>
#include <limits.h>
#include <memory>
#include <assert.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include "create_files_index.h"
#include <stack>
#include "FullFileBackup.h"
#include "IncrFileBackup.h"
#include "ImageBackup.h"
#include "ContinuousBackup.h"
#include "ThrottleUpdater.h"
#include "../fileservplugin/IFileServ.h"
#include "DataplanDb.h"
#include "stdlib.h"
#include <fstream>
#include <jsoncpp/json/json.h>

extern IUrlFactory *url_fak;
extern ICryptoFactory *crypto_fak;
extern std::string server_identity;
extern std::string server_token;
extern IFileServ* fileserv;

const unsigned short serviceport=35623;
const unsigned int check_time_intervall=5*60*1000;
const unsigned int status_update_intervall=1000;
const unsigned int eta_update_intervall=60000;
const size_t minfreespace_min=50*1024*1024;
const unsigned int ident_err_retry_time=1*60*1000;
const unsigned int ident_err_retry_time_retok=10*60*1000;
const unsigned int c_filesrv_connect_timeout=10000;
const unsigned int c_internet_fileclient_timeout=30*60*1000;
const unsigned int c_sleeptime_failed_imagebackup=20*60;
const unsigned int c_sleeptime_failed_filebackup=20*60;
const unsigned int c_exponential_backoff_div=2;
const unsigned int c_image_cowraw_bit=1024;


int ClientMain::running_backups=0;
int ClientMain::running_file_backups=0;
IMutex *ClientMain::running_backup_mutex=NULL;
IMutex *ClientMain::tmpfile_mutex=NULL;
size_t ClientMain::tmpfile_num=0;
IMutex* ClientMain::cleanup_mutex = NULL;
std::map<int, std::vector<SShareCleanup> > ClientMain::cleanup_shares;
int ClientMain::restore_client_id = -1;
bool ClientMain::running_backups_allowed=true;



ClientMain::ClientMain(IPipe *pPipe, sockaddr_in pAddr, const std::string &pName,
	const std::string& pSubName, const std::string& pMainName, int filebackup_group_offset, bool internet_connection,
	bool use_file_snapshots, bool use_image_snapshots, bool use_reflink)
	: internet_connection(internet_connection), server_settings(NULL), client_throttler(NULL),
	  use_file_snapshots(use_file_snapshots), use_image_snapshots(use_image_snapshots), use_reflink(use_reflink),
	  backup_dao(NULL), client_updated_time(0), continuous_backup(NULL),
	  clientsubname(pSubName), filebackup_group_offset(filebackup_group_offset), needs_authentification(false),
	restore_mutex(Server->createMutex())
{
	q_update_lastseen=NULL;
	pipe=pPipe;
	clientaddr=pAddr;
	clientaddr_mutex=Server->createMutex();
	clientname=pName;
	clientmainname=pMainName;
	clientid=0;

	do_full_backup_now=false;
	do_incr_backup_now=false;
	do_update_settings=false;
	do_full_image_now=false;
	do_incr_image_now=false;
	do_update_access_key = false;
	cdp_needs_sync=true;

	can_backup_images=true;

	protocol_versions.file_protocol_version=1;
	update_version=0;

	tcpstack.setAddChecksum(internet_connection);

	last_backup_try = 0;

	last_image_backup_try=0;
	count_image_backup_try=0;

	last_file_backup_try=0;
	count_file_backup_try=0;

	last_cdp_backup_try=0;
	count_cdp_backup_try=0;

	continuous_mutex=Server->createMutex();
	throttle_mutex=Server->createMutex();	

	curr_image_version=1;
	last_incr_freq=-1;
	last_startup_backup_delay = -1;

	curr_server_token = server_token;
	if(filebackup_group_offset>0)
	{
		curr_server_token+=convert(filebackup_group_offset/c_group_size);
	}

	session_identity_refreshtime = 0;
	connection_metered = false;
	do_reauthenticate = false;
}

ClientMain::~ClientMain(void)
{
	if(q_update_lastseen!=NULL)
		unloadSQL();

	Server->destroy(clientaddr_mutex);

	if(client_throttler!=NULL)
	{
		Server->destroy(client_throttler);
	}

	Server->destroy(continuous_mutex);
	Server->destroy(throttle_mutex);
}

void ClientMain::init_mutex(void)
{
	running_backup_mutex=Server->createMutex();
	tmpfile_mutex=Server->createMutex();
	cleanup_mutex=Server->createMutex();
}

void ClientMain::destroy_mutex(void)
{
	Server->destroy(running_backup_mutex);
	Server->destroy(tmpfile_mutex);
	Server->destroy(cleanup_mutex);
}

void ClientMain::unloadSQL(void)
{
	db->destroyQuery(q_update_lastseen);
	db->destroyQuery(q_update_setting);
	db->destroyQuery(q_insert_setting);
	db->destroyQuery(q_get_setting);
	db->destroyQuery(q_get_unsent_logdata);
	db->destroyQuery(q_set_logdata_sent);
}

void ClientMain::operator ()(void)
{
	if(!sendServerIdentity(true))
	{
		pipe->Write("ok");
		Server->Log("client_main Thread for client \"" + clientname + "\" finished and the identity was not recognized", LL_INFO);
		delete this;
		return;
	}

	if( clientname.find("##restore##")==0 )
	{
		{
			IScopedLock lock(cleanup_mutex);
			clientid = restore_client_id--;
		}

		ServerChannelThread channel_thread(this, clientname, clientid, internet_connection, true, server_identity, curr_server_token, clientsubname);
		THREADPOOL_TICKET channel_thread_id=Server->getThreadPool()->execute(&channel_thread, "client channel");

		while(true)
		{
			std::string msg;
			pipe->Read(&msg);
			if(msg=="exit" || msg=="exitnow" )
				break;
		}

		channel_thread.doExit();
		Server->getThreadPool()->waitFor(channel_thread_id);

		cleanupShares();

		pipe->Write("ok");
		Server->Log("client_main Thread for client "+clientname+" finished, restore thread");
		delete this;
		return;
	}
	else
	{
		if(!authenticateIfNeeded(true, false))
		{
			pipe->Write("ok");
			delete this;
			return;
		}
	}

	std::string identity = getIdentity();

	db=Server->getDatabase(Server->getThreadID(), IDRIVEBMRDB_SERVER);
	DBScopedFreeMemory free_db_memory(db);

	std::auto_ptr<ServerBackupDao> local_server_backup_dao(new ServerBackupDao(db));
	backup_dao = local_server_backup_dao.get();

	server_settings=new ServerSettings(db);

	clientid=getClientID(db, clientname, server_settings, NULL);

	if(clientid==-1)
	{
		ServerStatus::setStatusError(clientname, se_too_many_clients);
		Server->Log("client_main Thread for client "+clientname+" finished, because there were too many clients", LL_INFO);

		Server->wait(10*60*1000); //10min

		BackupServer::forceOfflineClient(clientname);
		pipe->Write("ok");
		ServerLogger::reset(logid);
		delete server_settings;
		delete this;
		return;
	}
	else if(!clientsubname.empty())
	{
		backup_dao->setVirtualMainClient(clientmainname, clientid);
	}

	logid = ServerLogger::getLogId(clientid);

	delete server_settings;
	server_settings=new ServerSettings(db, clientid);

	if(!createDirectoryForClient())
	{
		int64 starttime = Server->getTimeMS();
		int64 ctime;
		bool has_dir=false;
		while( (ctime=Server->getTimeMS())>=starttime && ctime-starttime<10*60*1000)
		{
			Server->wait(1000);

			if(createDirectoryForClient())
			{
				has_dir=true;
				break;
			}
		}

		if(!has_dir)
		{
			BackupServer::forceOfflineClient(clientname);
			pipe->Write("ok");
			delete server_settings;
			delete this;
			return;
		}
	}

	if(server_settings->getSettings()->computername.empty() || !clientsubname.empty())
	{
		server_settings->getSettings()->computername=clientname;
	}

	if(server_settings->getImageFileFormat()==image_file_format_cowraw)
	{
		curr_image_version = curr_image_version & c_image_cowraw_bit;
	}
	else
	{
		curr_image_version = curr_image_version & ~c_image_cowraw_bit;
	}

	prepareSQL();

	int64 lastseen = Server->getTimeSeconds();
	updateLastseen(lastseen);	
		
	if(!updateCapabilities())
	{
		Server->Log("Could not get client capabilities", LL_ERROR);

		Server->wait(5*60*1000); //5min

		pipe->Write("ok");
		BackupServer::forceOfflineClient(clientname);
		delete server_settings;
		delete this;
		return;
	}

	ServerStatus::setClientId(clientname, clientid);

	bool use_reflink=false;
#ifndef _WIN32
	if( use_file_snapshots )
		use_reflink=true;
#endif
	use_tmpfiles=server_settings->getSettings()->use_tmpfiles;
	use_tmpfiles_images=server_settings->getSettings()->use_tmpfiles_images;
	if(!use_tmpfiles)
	{
		tmpfile_path=server_settings->getSettings()->backupfolder+os_file_sep()+"idrivebmr_tmp_files";
		os_create_dir(tmpfile_path);
		if(!os_directory_exists(tmpfile_path))
		{
			Server->Log("Could not create or see temporary folder in backuppath", LL_ERROR);
			use_tmpfiles=true;
		}
	}

	ServerChannelThread channel_thread(this, clientname, clientid, internet_connection, false, identity, curr_server_token, clientsubname);
	THREADPOOL_TICKET channel_thread_id=Server->getThreadPool()->execute(&channel_thread, "client channel");

	bool received_client_settings=true;
	ServerLogger::Log(logid, "Getting client settings...", LL_DEBUG);
	bool settings_doesnt_exist=false;
	if(server_settings->getSettings()->allow_overwrite && !getClientSettings(settings_doesnt_exist))
	{
		if(!settings_doesnt_exist)
		{
			ServerLogger::Log(logid, "Getting client settings failed. Retrying...", LL_INFO);
			Server->wait(200000);
			if(!getClientSettings(settings_doesnt_exist))
			{
				ServerLogger::Log(logid, "Getting client settings failed -1", LL_ERROR);
				received_client_settings=false;
			}
		}
		else
		{
			ServerLogger::Log(logid, "Getting client settings failed. Not retrying because settings do not exist.", LL_INFO);
		}
	}

	if(received_client_settings || settings_doesnt_exist)
	{
		sendSettings();
	}

	if(!server_settings->getSettings()->virtual_clients.empty())
	{
		BackupServer::setVirtualClients(clientname, server_settings->getSettings()->virtual_clients);
	}

	ServerLogger::Log(logid, "Sending backup incr interval...", LL_DEBUG);
	sendClientBackupIncrIntervall();

	int64 last_client_update_check = 0;
	if(server_settings->getSettings()->autoupdate_clients)
	{
		checkClientVersion();
		last_client_update_check = Server->getTimeMS();
	}

	sendClientLogdata();

	curr_image_format = server_settings->getImageFileFormat();

	ServerStatus::setCommPipe(clientname, pipe);

	bool skip_checking=false;

	if( server_settings->getSettings()->startup_backup_delay>0 )
	{
		pipe->isReadable(server_settings->getSettings()->startup_backup_delay*1000);
		skip_checking=true;
	}

	ServerSettings server_settings_updated(db, clientid);

	bool do_exit_now=false;
	
	while(true)
	{
		if(!skip_checking)
		{
			timeoutRestores();

			bool send_logdata = false;

			for (size_t i = 0; i<backup_queue.size();)
			{
				if (backup_queue[i].ticket != ILLEGAL_THREADPOOL_TICKET)
				{
					if (!backup_queue[i].backup->isFileBackup())
					{
						ImageBackup* ibackup = dynamic_cast<ImageBackup*>(backup_queue[i].backup);
						if (ibackup != NULL)
						{
							std::vector<ImageBackup::SImageDependency> dependencies = ibackup->getDependencies(true);
							if (!dependencies.empty())
							{
								std::map<ImageBackup*, bool> new_running_image_group;
								new_running_image_group[ibackup] = false;

								for (size_t j = 0; j < dependencies.size(); ++j)
								{
									std::string vols_list = ibackup->getLetter();
									for (size_t k = 0; k < dependencies.size(); ++k)
									{
										if (j == k) continue;
										vols_list += ", " + dependencies[k].volume;
									}

									std::string letter = normalizeVolumeUpper(dependencies[j].volume);

									SRunningBackup backup;
									ImageBackup* idep = new ImageBackup(this, clientid, clientname, clientsubname, LogAction_LogIfNotDisabled,
										ibackup->isIncrementalBackup(), letter, curr_server_token, letter, false,
										dependencies[j].snapshot_id, vols_list, ibackup->getBackupStarttime(), ibackup->isScheduled());
									backup.backup = idep;
									backup.letter = letter;

									new_running_image_group[idep] = false;

									backup_queue.push_back(backup);
								}

								running_image_groups.push_back(new_running_image_group);
							}
						}
					}

					if ( Server->getThreadPool()->waitFor(backup_queue[i].ticket, 0)
						 && (dynamic_cast<ImageBackup*>(backup_queue[i].backup)==NULL 
							 || dynamic_cast<ImageBackup*>(backup_queue[i].backup)->getDependencies(false).empty()) )
					{
						ServerStatus::subRunningJob(clientmainname);


						if (!backup_queue[i].backup->getResult() &&
							backup_queue[i].backup->shouldBackoff())
						{
							if (backup_queue[i].backup->isFileBackup())
							{
								last_file_backup_try = Server->getTimeMS();
								++count_file_backup_try;
								ServerLogger::Log(logid, "Exponential backoff: Waiting at least " + PrettyPrintTime(exponentialBackoffTimeFile() * 1000) + " before next file backup", LL_WARNING);
							}
							else
							{
								last_image_backup_try = Server->getTimeMS();
								++count_image_backup_try;
								ServerLogger::Log(logid, "Exponential backoff: Waiting at least " + PrettyPrintTime(exponentialBackoffTimeImage() * 1000) + " before next image backup", LL_WARNING);
							}
						}
						else
						{
							if (backup_queue[i].backup->isFileBackup())
							{
								count_file_backup_try = 0;
							}
							else
							{
								count_image_backup_try = 0;
							}

							last_backup_try = Server->getTimeMS();
						}

						bool del_backup = true;

						if (!backup_queue[i].backup->isFileBackup())
						{
							ImageBackup* ibackup = dynamic_cast<ImageBackup*>(backup_queue[i].backup);
							if (ibackup != NULL)
							{
								for (size_t j = 0; j < running_image_groups.size();)
								{
									{

										std::map<ImageBackup*, bool>::iterator it = running_image_groups[j].find(ibackup);
										if (it != running_image_groups[j].end())
										{
											ServerCleanupThread::lockImageFromCleanup(ibackup->getBackupId());
											del_backup = false;
											it->second = true;
										}
									}

									bool finished = true;
									bool success = true;
									for (std::map<ImageBackup*, bool>::iterator it = running_image_groups[j].begin();
										it != running_image_groups[j].end(); ++it)
									{

										if (!it->second)
										{
											finished = false; //running_image_groups is false
										}
										else if (!it->first->getResult())
										{
											success = false;//do_backup() fails for a drive
										}
									}

									if (finished)
									{
										for (std::map<ImageBackup*, bool>::iterator it = running_image_groups[j].begin();
											it != running_image_groups[j].end(); ++it)
										{
											if (success)
											{
												backup_dao->setImageBackupComplete(it->first->getBackupId());
												backup_dao->updateClientLastImageBackup(it->first->getBackupId(), clientid);
											}
											ServerCleanupThread::unlockImageFromCleanup(it->first->getBackupId());
											delete it->first;
										}

										running_image_groups.erase(running_image_groups.begin() + j);
										continue;
									}

									++j;
								}
							}
						}



						if (del_backup)
						{
							delete backup_queue[i].backup;
						}

						send_logdata = true;
						backup_queue.erase(backup_queue.begin() + i);
						GetClientLogID();
						if(backup_queue.empty() || count_image_backup_try )
						{
							int64 cpassed_time_s;
							for(int i=0; i<allBackupsInDatabase.size(); i++)
								Server->Log("BackupIds : " + convert(allBackupsInDatabase[i]), LL_INFO);
							if (allBackupsInDatabase.size())
							{
								InvokePostBackupScripts(allBackupsInDatabase);
								virtualizationLogid.clear();
							}
							allBackupsInDatabase.clear();
							//Server->Log("Clearing allBackupsInDatabase", LL_INFO);
						}
						continue;
					}
				}

				++i;
			}

			if (send_logdata)
			{
				sendClientLogdata();
			}

			{
				bool received_client_settings=true;
				bool settings_updated=false;
				server_settings_updated.getSettings(&settings_updated);
				bool settings_dont_exist=false;
				if(do_update_settings || settings_updated)
				{
					ServerLogger::Log(logid, "Getting client settings...", LL_DEBUG);
					do_update_settings=false;
					if(server_settings->getSettings()->allow_overwrite && !getClientSettings(settings_dont_exist))
					{
						ServerLogger::Log(logid, "Getting client settings failed -2", LL_ERROR);
						received_client_settings=false;
					}

					if(server_settings->getImageFileFormat()==image_file_format_cowraw)
					{
						curr_image_version = curr_image_version & c_image_cowraw_bit;
					}
					else
					{
						curr_image_version = curr_image_version & ~c_image_cowraw_bit;
					}

					if(!server_settings->getSettings()->virtual_clients.empty())
					{
						BackupServer::setVirtualClients(clientname, server_settings->getSettings()->virtual_clients);
					}
				}

				if(settings_updated && (received_client_settings || settings_dont_exist) )
				{
					sendSettings();
				}

				if(settings_updated)
				{
					sendClientBackupIncrIntervall();
				}
			}

			if( (client_updated_time!=0 && Server->getTimeMS()-client_updated_time>6*60*1000)
				|| update_capa)
			{
				update_capa = false;

				updateCapabilities();
				client_updated_time=0;
				session_identity_refreshtime = 0;
				if (!authenticateIfNeeded(true, false))
				{
					skip_checking = true;
				}
			}

			if (do_reauthenticate)
			{
				do_reauthenticate = false;
				if ( !sendServerIdentity(true)
				 	  || !authenticateIfNeeded(true, true) )
				{
					skip_checking = true;
				}
			}

			if (Server->getTimeMS()- last_client_update_check > 24*60*60*1000
				&& server_settings->getSettings()->autoupdate_clients)
			{
				checkClientVersion();
				last_client_update_check = Server->getTimeMS();
			}

			if (Server->getTimeSeconds() - lastseen > 5 * 60
				&& ServerStatus::getLastseen(clientname)>lastseen)
			{
				lastseen = ServerStatus::getLastseen(clientname);
				updateLastseen(lastseen);
			}

			curr_image_format = server_settings->getImageFileFormat();


			bool internet_no_full_file=(internet_connection && !server_settings->getSettings()->internet_full_file_backups );
			bool internet_no_images=(internet_connection && !server_settings->getSettings()->internet_image_backups );

			if(do_incr_image_now)
			{
				if(!can_backup_images)
					ServerLogger::Log(logid, "Cannot do image backup because can_backup_images=false", LL_DEBUG);
				if(server_settings->getSettings()->no_images)
					ServerLogger::Log(logid, "Cannot do image backup because no_images=true", LL_DEBUG);
				if(!isBackupsRunningOkay(false))
					ServerLogger::Log(logid, "Cannot do image backup because isBackupsRunningOkay()=false", LL_DEBUG);
				if(!internet_no_images )
					ServerLogger::Log(logid, "Cannot do image backup because internet_no_images=true", LL_DEBUG);
			}

			if(do_incr_backup_now)
			{
				if(server_settings->getSettings()->no_file_backups)
					ServerLogger::Log(logid, "Cannot do incremental file backup because no_file_backups=true", LL_DEBUG);
				if(!isBackupsRunningOkay(true))
					ServerLogger::Log(logid, "Cannot do incremental file backup because isBackupsRunningOkay()=false", LL_DEBUG);
			}

			if (do_update_access_key)
			{
				do_update_access_key = false;
				updateClientAccessKey();
			}

			if( !server_settings->getSettings()->no_file_backups && (!internet_no_full_file || do_full_backup_now) &&
				( (isUpdateFull(filebackup_group_offset + c_group_default) && ServerSettings::isInTimeSpan(server_settings->getBackupWindowFullFile())
				&& exponentialBackoffFile() && pauseRetryBackup() && isDataplanOkay(true) && isOnline(channel_thread) ) || do_full_backup_now )
				&& isBackupsRunningOkay(true) && !do_full_image_now && !do_incr_image_now && !do_incr_backup_now
				&& (!isRunningFileBackup(filebackup_group_offset + c_group_default) || do_full_backup_now) )
			{
				SRunningBackup backup;
				backup.backup = new FullFileBackup(this, clientid, clientname, clientsubname,
					do_full_backup_now?LogAction_AlwaysLog:LogAction_LogIfNotDisabled, filebackup_group_offset + c_group_default, use_tmpfiles,
					tmpfile_path, use_reflink, use_file_snapshots, curr_server_token, convert(c_group_default), !do_full_backup_now);
				backup.group=filebackup_group_offset + c_group_default;

				backup_queue.push_back(backup);

				do_full_backup_now=false;
			}
			else if( !server_settings->getSettings()->no_file_backups
				&& ( (isUpdateIncr(filebackup_group_offset + c_group_default) && ServerSettings::isInTimeSpan(server_settings->getBackupWindowIncrFile())
				&& exponentialBackoffFile() && pauseRetryBackup() && isDataplanOkay(true) && isOnline(channel_thread) ) || do_incr_backup_now )
				&& isBackupsRunningOkay(true) && !do_full_image_now && !do_incr_image_now
				&& (!isRunningFileBackup(filebackup_group_offset + c_group_default) || do_incr_backup_now) )
			{
				SRunningBackup backup;
				backup.backup = new IncrFileBackup(this, clientid, clientname, clientsubname,
					do_full_backup_now?LogAction_AlwaysLog:LogAction_LogIfNotDisabled, filebackup_group_offset + c_group_default, use_tmpfiles,
					tmpfile_path, use_reflink, use_file_snapshots, curr_server_token, convert(c_group_default), !do_incr_backup_now);
				backup.group=filebackup_group_offset + c_group_default;

				backup_queue.push_back(backup);

				do_incr_backup_now=false;
			}
			else if(can_backup_images && !server_settings->getSettings()->no_images && (!internet_no_images || do_full_image_now)
				&& ( (isUpdateFullImage() && ServerSettings::isInTimeSpan(server_settings->getBackupWindowFullImage())
				&& exponentialBackoffImage() && pauseRetryBackup() && isDataplanOkay(false) && isOnline(channel_thread) ) || do_full_image_now)
				&& isBackupsRunningOkay(false) && !do_incr_image_now)
			{

				std::vector<std::string> vols=server_settings->getBackupVolumes(all_volumes, all_nonusb_volumes);
				for(size_t i=0;i<vols.size();++i)
				{
					std::string letter=normalizeVolumeUpper(vols[i]);
					if( ( (isUpdateFullImage(letter) && !isRunningImageBackup(letter) && isBackupsRunningOkay(false)) || do_full_image_now)
						&& !isImageGroupQueued(letter, true) )
					{
						SRunningBackup backup;
						backup.backup = new ImageBackup(this, clientid, clientname, clientsubname,
							do_full_image_now?LogAction_AlwaysLog:LogAction_LogIfNotDisabled,
							false, letter, curr_server_token, letter, true, 0, std::string(), 0, !do_full_image_now);
						backup.letter=letter;

						backup_queue.push_back(backup);
					}
				}

				do_full_image_now=false;
			}
			else if(can_backup_images && !server_settings->getSettings()->no_images && (!internet_no_images || do_incr_image_now)
				&& ((isUpdateIncrImage() && ServerSettings::isInTimeSpan(server_settings->getBackupWindowIncrImage()) 
				&& exponentialBackoffImage() && pauseRetryBackup() && isDataplanOkay(false) && isOnline(channel_thread) && PreviousVirtualizeVerificationComplete()) || do_incr_image_now)
				&& isBackupsRunningOkay(false) )
			{
				std::vector<std::string> vols=server_settings->getBackupVolumes(all_volumes, all_nonusb_volumes);
				for(size_t i=0;i<vols.size();++i)
				{
					std::string letter= normalizeVolumeUpper(vols[i]);
					if( ((isUpdateIncrImage(letter) && !isRunningImageBackup(letter) && isBackupsRunningOkay(false) ) || do_incr_image_now)
						&& !isImageGroupQueued(letter, false) )
					{
						SRunningBackup backup;
						backup.backup = new ImageBackup(this, clientid, clientname, clientsubname, do_incr_image_now ?LogAction_AlwaysLog:LogAction_LogIfNotDisabled,
							true, letter, curr_server_token, letter, true, 0, std::string(), 0, !do_incr_image_now);
						backup.letter=letter;

						backup_queue.push_back(backup);
					}
				}

				do_incr_image_now=false;
			}
			else if(protocol_versions.cdp_version>0 && cdp_needs_sync && !isRunningFileBackup(filebackup_group_offset + c_group_continuous))
			{
				cdp_needs_sync=false;

				SRunningBackup backup;
				backup.backup = new ContinuousBackup(this, clientid, clientname, clientsubname,
					LogAction_LogIfNotDisabled, filebackup_group_offset + c_group_continuous, use_tmpfiles,
					tmpfile_path, use_reflink, use_file_snapshots, convert(c_group_continuous), false);
				backup.group=filebackup_group_offset + c_group_continuous;

				backup_queue.push_back(backup);
			}

			bool can_start=false;
			size_t running_jobs=0;
			for(size_t i=0;i<backup_queue.size();++i)
			{
				if(backup_queue[i].ticket==ILLEGAL_THREADPOOL_TICKET)
				{
					can_start=true;
				}
			}

			if(can_start)
			{
				while(ServerStatus::numRunningJobs(clientmainname)<server_settings->getSettings()->max_running_jobs_per_client)
				{
					bool started_job=false;
					for(size_t i=0;i<backup_queue.size();++i)
					{
						bool filebackup = dynamic_cast<FileBackup*>(backup_queue[i].backup) != NULL;

						if( backup_queue[i].ticket==ILLEGAL_THREADPOOL_TICKET
							&& (!backup_queue[i].backup->isScheduled() || inBackupWindow(backup_queue[i].backup) )
							&& (!filebackup || !isRunningFileBackup(backup_queue[i].group, false) ) )
						{
							ServerStatus::addRunningJob(clientmainname);
							if(ServerStatus::numRunningJobs(clientmainname)<=server_settings->getSettings()->max_running_jobs_per_client
								&& isBackupsRunningOkay(filebackup, true))
							{
								std::string tname = "backup main";
								if (filebackup)
								{
									tname = "fbackup main";
								}
								else if (dynamic_cast<ImageBackup*>(backup_queue[i].backup) != NULL)
								{
									tname = "ibackup main";
								}

								backup_queue[i].ticket=Server->getThreadPool()->execute(backup_queue[i].backup, tname);
								started_job=true;
							}
							else
							{
								ServerStatus::subRunningJob(clientmainname);
							}							
							break;
						}
					}

					if(!started_job)
					{
						break;
					}
				}
			}
		}

		std::string msg;
		pipe->Read(&msg, skip_checking?0:check_time_intervall);
		
		skip_checking=false;
		if(msg=="exit")
			break;
		else if(msg=="exitnow")
		{
			do_exit_now=true;
			break;
		}
		else if(msg=="START BACKUP INCR") do_incr_backup_now=true;
		else if(msg=="START BACKUP FULL") do_full_backup_now=true;
		else if(msg=="UPDATE SETTINGS") do_update_settings=true;
		else if(msg=="START IMAGE INCR") do_incr_image_now=true;
		else if(msg=="START IMAGE FULL") do_full_image_now=true;
		else if (msg == "UPDATE ACCESS KEY") do_update_access_key = true;
		else if(next(msg, 0, "address"))
		{
			updateClientAddress(msg.substr(7));

			tcpstack.setAddChecksum(internet_connection);

			if(!authenticateIfNeeded(true, true))
			{
				skip_checking=true;
			}
		}
		else if(msg=="WAKEUP")
		{

		}
		else if(next(msg, 0, "RESTORE"))
		{
			std::string data_str = msg.substr(7);
			CRData rdata(&data_str);

			std::string restore_identity;
			rdata.getStr(&restore_identity);
			int64 restore_id;
			rdata.getInt64(&restore_id);
			int64 status_id;
			rdata.getInt64(&status_id);
			logid_t log_id = logid_t();
			rdata.getInt64(&log_id.first);
			std::string restore_token;
			rdata.getStr(&restore_token);
			char single_file = 0;
			rdata.getChar(&single_file);
			char clean_other = 0;
			rdata.getChar(&clean_other);
			char ignore_other_fs = 0;
			rdata.getChar(&ignore_other_fs);
			int64 restore_flags = 0;
			rdata.getInt64(&restore_flags);

			std::string restore_path = ServerStatus::getProcess(clientname, status_id).details;

			bool server_confirms;
			if(ServerStatus::canRestore(clientname, server_confirms))
			{
				if(server_confirms || !restore_token.empty())
				{
					ServerLogger::Log(log_id, "Starting restore of path \""+ restore_path + "\"...", LL_INFO);
				}
				else
				{
					ServerLogger::Log(log_id, "Starting restore of path \"" + restore_path + "\". Waiting for client confirmation...", LL_INFO);
				}
			}
			else
			{
				ServerLogger::Log(log_id, "Starting restore of path \"" + restore_path + "\". But client may be offline...", LL_INFO);
			}

			if (crypto_fak == NULL)
			{
				ServerLogger::Log(log_id, "Cannot restore without crypto plugin", LL_ERROR);
				restore_identity.clear();
			}
			else
			{
				restore_identity = base64_encode_dash(crypto_fak->encryptAuthenticatedAES(restore_identity, server_settings->getSettings()->client_access_key, 1));
			}

			std::string ret = sendClientMessageRetry("FILE RESTORE client_token="+restore_identity+"&server_token="+curr_server_token+
				"&id="+convert(restore_id)+"&status_id="+convert(status_id)+
				"&log_id="+convert(log_id.first)+(restore_token.empty()?"":"&restore_token="+restore_token)+
				"&restore_path="+EscapeParamString(restore_path)+
				"&single_file="+ convert(static_cast<int>(single_file))+
				"&clean_other=" + convert(static_cast<int>(clean_other))+
				"&ignore_other_fs=" + convert(static_cast<int>(ignore_other_fs))+
				"&restore_flags="+convert(restore_flags)+
				"&tgroup=0"
				"&clientsubname="+EscapeParamString(clientsubname),
				"Starting restore failed", 10000, 10, true, LL_ERROR);

			if (ret != "ok")
			{
				ServerLogger::Log(log_id, "Starting restore failed: "+ret, LL_ERROR);

				finishFailedRestore(restore_identity, log_id, status_id, restore_id);
			}
			else
			{
				IScopedLock lock(restore_mutex.get());

				running_restores.push_back(SRunningRestore(restore_identity, log_id, status_id, restore_id));
			}
		}

		if(!msg.empty())
		{
			Server->Log("msg="+msg, LL_DEBUG);
		}
	}

	Server->Log("Waiting for backup threads to finish (clientid="+convert(clientid)+", nthreads=" + convert(backup_queue.size())+")...", LL_DEBUG);
	for(size_t i=0;i<backup_queue.size();++i)
	{
		if(backup_queue[i].ticket!=ILLEGAL_THREADPOOL_TICKET)
		{
			Server->getThreadPool()->waitFor(backup_queue[i].ticket);
			ServerStatus::subRunningJob(clientmainname);
		}

		delete backup_queue[i].backup;
	}

	Server->wait(2000);


	ServerStatus::setCommPipe(clientname, NULL);

	//destroy channel
	{
		Server->Log("Stopping channel...", LL_DEBUG);
		channel_thread.doExit();
		Server->getThreadPool()->waitFor(channel_thread_id);
	}

	cleanupShares();

	ServerLogger::reset(clientid);
	
	
	delete server_settings;
	server_settings=NULL;
	pipe->Write("ok");
	Server->Log("client_main Thread for client "+clientname+" finished");

	delete this;
}
static void SetConstantParameters(JSON::Object &virtObject)
{
	std::string cpus = "4";
	std::string memory = "4096";
	std::string nwStatus = "Disconnected";
	std::string nwType = "None";
	std::string storController = "SATA";
	std::string video = "QXL";

	virtObject.set("vcpu", cpus);
	virtObject.set("memory", memory);
	JSON::Object networkParams;
	networkParams.set("nw-source", nwStatus);
	networkParams.set("nw-type", nwType);
	virtObject.set("network_params",networkParams);
	virtObject.set("stor_controller", storController);
	virtObject.set("video", video);
}

bool ClientMain::ValidateVirtualization(std::vector<int> backupIds, JSON::Object &virtObject)
{
	int validateVirtualization = 3;
	bool completeStatus = true;
	std::vector<ServerBackupDao::SBackupImageInfo>
		backupInfo = backup_dao->getBackupInfo(backupIds);
	int clientid = -1;

	if(!backupInfo.size()) return false; //may be wrong

	JSON::Array volumeArray;
	for(int i=0; i<backupInfo.size(); i++)
	{
		if(backupInfo[i].complete != validateVirtualization) //is this check needed?
		{
			return false;
		}

		volumeArray.add(backupInfo[i].path);
		if(clientid == -1)
			clientid = backupInfo[i].clientId;
		else
			clientid = (clientid == backupInfo[i].clientId) ? clientid : -1;
	}

	//Get the hostname
	std::string backupPath(backupInfo[0].path);
	std::string storageString("/storage/idrivebmr/");
	std::size_t found = backupPath.find("/", storageString.length());
	std::string host;
	if (found!=std::string::npos)
	{
		int len = found - storageString.length();
		host = backupPath.substr(storageString.length(), len);
	}

	SetConstantParameters(virtObject);
	virtObject.set("volumes", volumeArray);
	virtObject.set("clientid", clientid);
	virtObject.set("hostname", host);

	JSON::Array logids,ids;
	for(auto v : virtualizationLogid)
		logids.add(v);

	for(auto backupId: backupIds)
		ids.add(backupId);

	virtualizationLogid.clear();
	virtObject.set("logids", logids);
	virtObject.set("backupIds", ids);
	for(int i=0; i<virtualizationLogid.size(); i++)
		Server->Log("in Validatevirt " + virtualizationLogid[i]);

	JSON::Object clientVirtData;
	clientVirtData.set("logids", logids);
	clientVirtData.set("backupIds", ids);
	clientVirtData.set("VirtStartTime", Server->getTimeSeconds());
	clientVirtData.set("VirtStatus", VIRT_PENDING);
	Server->Log("Client Virtualization data " + clientVirtData.stringify(false), LL_INFO);
	backup_dao->setVirtualizationStatus(clientid, clientVirtData.stringify(true));

	return true;
}

bool ClientMain::InvokePostBackupScripts(std::vector<int> backupInfo)
{
	JSON::Object backupJsondata;
	bool backupIntegSuccess = JsonizeRetrievedData(backupInfo, backupJsondata);
	if(!backupIntegSuccess)
	{
		//run postbackup.py script
		Server->Log("Postbackup data " + backupJsondata.stringify(false), LL_INFO);
		if (!ClientMain::run_script("idrivebmr" + os_file_sep() + "postbackup.py", "-in \'" + backupJsondata.stringify(true) + "\'", logid))
		{
			Server->Log("Error in postbackup.py script", LL_ERROR);
		}
		return false;
	}
	JSON::Object virtData;
	ValidateVirtualization(backupInfo, virtData);
	JSON::Object virtObject;
	virtObject.set("vm_params", virtData);
	virtObject.set("postbackup_data", backupJsondata);
	Server->Log("Assembled data "+ virtObject.stringify(true));
	std::string cmd = "/usr/local/bin/python3";
	std::string params = " /usr/share/idrivebmr/run Virtualization --verify-physical '" + virtObject.stringify(true) + "'";
	if (!ClientMain::run_script(cmd, params, logid))
	{
		Server->Log("Error in calling Virtualization script", LL_ERROR);
	}
	return true;
}

bool ClientMain::JsonizeRetrievedData(std::vector<int> backupIds, JSON::Object &recoveryId)
{
	std::vector<std::string> paths;
	JSON::Array backupDetail;

	int64 backupTime = 0;
	std::string timeStr;

	std::vector<ServerBackupDao::SBackupImageInfo>
		backupInfo = backup_dao->getBackupInfo(backupIds);

	bool completeStatus = true;
	int clientid = -1;
	std::map<std::string, std::string> drivesPaths;

	//ToBe tested
	if(!backupInfo.size()) completeStatus = false;

	for ( int i = 0; i < backupInfo.size(); i++)
	{
		std::string letter = backupInfo[i].letter;
		if(clientid == -1)
			clientid = backupInfo[i].clientId;
		else
			clientid = (clientid == backupInfo[i].clientId) ? clientid : -1;

		if(!backupTime && (letter != "SYSVOL" && letter != "ESP"))
		{
			timeStr.clear();
			backupTime = backupInfo[i].backuptime;
			timeStr = backup_dao->formatUnixtimeUTC(backupTime).value;
		}

		completeStatus = completeStatus & backupInfo[i].complete;
		drivesPaths.insert(std::pair<std::string, std::string>(backupInfo[i].letter, backupInfo[i].path));

		JSON::Object backupobj;
		backupobj.set("drive", letter);
		backupobj.set("path", backupInfo[i].path);
		backupobj.set("backup_type", backupInfo[i].incremental ? "Incremental" : "Full");
		backupDetail.add(backupobj);
	}

	recoveryId.set("recoveryID", timeStr);
	recoveryId.set("status", completeStatus ? "Success" : "Failed");
	bool integStatus = false;
	if(completeStatus)
	{
		integStatus = GetIntegrityStatus(drivesPaths);
		recoveryId.set("integrity", integStatus ? "Good" : "Bad");
	}
	else
		recoveryId.set("integrity", "Bad");
	recoveryId.set("backups", backupDetail);
	recoveryId.set("clientId", convert(clientid));

	return(completeStatus && integStatus);
}

bool ClientMain::GetIntegrityStatus(std::map<std::string, std::string> drivespaths)
{
	std::string mount_dir = "/tmp/test";
	std::string filesysType = "lowntfs-3g";
	std::string backedUpFile = "";
	for(std::map<std::string, std::string>::iterator it = drivespaths.begin();
			it != drivespaths.end(); ++it)
	{
		std::string mount_point = mount_dir + convert(rand());
		mkdir(mount_point.c_str(), 0777);
		backedUpFile = it->second;
		if(it->first == "ESP")
			filesysType = "vfat";

		int rc = system(("sudo mount -t " + filesysType + " -o loop,offset=524288,ro,noexec " + backedUpFile + " " + mount_point).c_str());

		if(it->first == "SYSVOL" && rc != 0){
		       	filesysType = "vfat";	
			rc = system(("sudo mount -t " + filesysType + " -o loop,offset=524288,ro,noexec " + backedUpFile + " " + mount_point).c_str());
		}

		if(rc!=0)
		{
			Server->Log("Backup integrity check failed. " + backedUpFile, LL_WARNING);
			return false;
		}
		else
		{
			rc = system(("sudo umount -l " + backedUpFile).c_str());
			if(rc)
			{
				Server->Log("Unmount Failed " + backedUpFile);
				return false;
			}
			Server->Log("PASSED backup integrity check for " + backedUpFile, LL_INFO);
		}
		filesysType = "lowntfs-3g";
		rmdir(mount_point.c_str());
	}
	return true;
}

void ClientMain::prepareSQL(void)
{
	q_update_lastseen=db->Prepare("UPDATE clients SET lastseen=datetime(?, 'unixepoch') WHERE id=?", false);
	q_update_setting=db->Prepare("UPDATE settings_db.settings SET value=? WHERE key=? AND clientid=?", false);
	q_insert_setting=db->Prepare("INSERT INTO settings_db.settings (key, value, clientid) VALUES (?,?,?)", false);
	q_get_setting = db->Prepare("SELECT value FROM settings_db.settings WHERE clientid=? AND key=?", false);
	q_get_unsent_logdata=db->Prepare("SELECT l.id AS id, strftime('%s', l.created) AS created, log_data.data AS logdata FROM (logs l INNER JOIN log_data ON l.id=log_data.logid) WHERE sent=0 AND clientid=?", false);
	q_set_logdata_sent=db->Prepare("UPDATE logs SET sent=1 WHERE id=?", false);
}

int ClientMain::getClientID(IDatabase *db, const std::string &clientname, ServerSettings *server_settings, bool *new_client, std::string* authkey)
{
	if(new_client!=NULL)
		*new_client=false;

	IQuery *q=db->Prepare("SELECT id FROM clients WHERE name=?",false);
	if(q==NULL) return -1;

	q->Bind(clientname);
	db_results res=q->Read();
	db->destroyQuery(q);

	if(res.size()>0)
		return watoi(res[0]["id"]);
	else
	{
		IQuery *q_get_num_clients=db->Prepare("SELECT count(*) AS c FROM clients WHERE lastseen > date('now', '-2 month')", false);
		db_results res_r=q_get_num_clients->Read();
		q_get_num_clients->Reset();
		int c_clients=-1;
		if(!res_r.empty()) c_clients=watoi(res_r[0]["c"]);

		db->destroyQuery(q_get_num_clients);

		if(server_settings==NULL || c_clients<server_settings->getSettings()->max_active_clients)
		{
			DBScopedWriteTransaction trans(db);
			IQuery *q_insert_newclient=db->Prepare("INSERT INTO clients (name, lastseen,bytes_used_files,bytes_used_images,created) VALUES (?, CURRENT_TIMESTAMP, 0, 0, strftime('%s', 'now') )", false);
			q_insert_newclient->Bind(clientname);
			q_insert_newclient->Write();
			int rid=(int)db->getLastInsertID();
			q_insert_newclient->Reset();
			db->destroyQuery(q_insert_newclient);

			IQuery *q_insert_authkey=db->Prepare("INSERT INTO settings_db.settings (key,value, clientid) VALUES ('internet_authkey',?,?)", false);
			std::string new_authkey = ServerSettings::generateRandomAuthKey();
			q_insert_authkey->Bind(new_authkey);
			q_insert_authkey->Bind(rid);
			q_insert_authkey->Write();
			q_insert_authkey->Reset();
			db->destroyQuery(q_insert_authkey);

			if (authkey != NULL)
			{
				*authkey = new_authkey;
			}

			if(new_client!=NULL)
				*new_client=true;

			return rid;
		}
		else
		{
			Server->Log("Too many clients. Didn't accept client '"+clientname+"'", LL_INFO);
			return -1;
		}
	}
}

void ClientMain::updateLastseen(int64 lastseen)
{
	q_update_lastseen->Bind(lastseen);
	q_update_lastseen->Bind(clientid);
	q_update_lastseen->Write();
	q_update_lastseen->Reset();
}

bool ClientMain::isUpdateFull(int tgroup)
{
	int update_freq_full = server_settings->getUpdateFreqFileFull();
	if( update_freq_full<0 )
		return false;

	int update_freq_incr = server_settings->getUpdateFreqFileIncr();
	if(update_freq_incr<0)
		update_freq_incr=0;

	return !backup_dao->hasRecentFullOrIncrFileBackup(convert(-1*update_freq_full)+" seconds",
		clientid, convert(-1*update_freq_incr)+" seconds", tgroup).exists;
}

bool ClientMain::isUpdateIncr(int tgroup)
{
	int update_freq = server_settings->getUpdateFreqFileIncr();
	if( update_freq<0 )
		return false;

	return !backup_dao->hasRecentIncrFileBackup(convert(-1*update_freq)+" seconds",
		clientid, tgroup).exists;
}

bool ClientMain::isUpdateFullImage(const std::string &letter)
{
	int update_freq_full = server_settings->getUpdateFreqImageFull();
	if( update_freq_full<0 )
		return false;

	int update_freq_incr = server_settings->getUpdateFreqImageIncr();
	if(update_freq_incr<0)
		update_freq_incr=0;

	return !backup_dao->hasRecentFullOrIncrImageBackup(convert(-1*update_freq_full)+" seconds",
		clientid, convert(-1*update_freq_incr)+" seconds", curr_image_version, normalizeVolumeUpper(letter)).exists;
}

bool ClientMain::isUpdateFullImage(void)
{
	std::vector<std::string> vols=server_settings->getBackupVolumes(all_volumes, all_nonusb_volumes);
	for(size_t i=0;i<vols.size();++i)
	{
		if( isUpdateFullImage(vols[i]+":") )
		{
			return true;
		}
	}
	return false;
}

bool ClientMain::isUpdateIncrImage(void)
{
	std::vector<std::string> vols=server_settings->getBackupVolumes(all_volumes, all_nonusb_volumes);
	for(size_t i=0;i<vols.size();++i)
	{
		if( isUpdateIncrImage(vols[i]+":") )
		{
			return true;
		}
	}
	return false;
}

bool ClientMain::isUpdateIncrImage(const std::string &letter)
{
	int update_freq = server_settings->getUpdateFreqImageIncr();
	if( update_freq<0 )
		return false;

	return !backup_dao->hasRecentIncrImageBackup(convert(-1*update_freq)+" seconds",
		clientid, curr_image_version, normalizeVolumeUpper(letter)).exists;
}

std::string ClientMain::sendClientMessageRetry(const std::string &msg, const std::string &errmsg, unsigned int timeout,
	size_t retry, bool logerr, int max_loglevel, unsigned int timeout_after_first)
{
	std::string res;
	do
	{
		int64 starttime=Server->getTimeMS();
		res = sendClientMessage(msg, errmsg, timeout, logerr, retry>0 ? LL_DEBUG : max_loglevel);

		if(res.empty())
		{
			if(retry>0)
			{
				if (timeout_after_first > 0)
				{
					timeout = timeout_after_first;
				}
				--retry;

				int64 passed_time=Server->getTimeMS()-starttime;
				if(passed_time<timeout)
				{
					Server->wait(static_cast<unsigned int>(timeout-passed_time));
				}
			}
			else
			{
				return res;
			}
		}
	}
	while(res.empty());

	return res;
}

std::string ClientMain::sendClientMessage(const std::string &msg, const std::string &errmsg,
	unsigned int timeout, bool logerr, int max_loglevel, SConnection* conn)
{
	CTCPStack tcpstack(internet_connection);

	std::auto_ptr<IPipe> cc;
	if (conn != NULL
		&& conn->conn.get() != NULL
		&& conn->internet_connection == internet_connection)
	{
		cc.reset(conn->conn.release());
	}
	else
	{
		cc.reset(getClientCommandConnection(NULL, 10000));
		if (cc.get() == NULL)
		{
			if (logerr)
				ServerLogger::Log(logid, "Connecting to ClientService of \"" + clientname + "\" failed: " + errmsg, max_loglevel);
			else
				Server->Log("Connecting to ClientService of \"" + clientname + "\" failed: " + errmsg, max_loglevel);
			return "";
		}
	}
	if (conn != NULL)
	{
		conn->conn.reset();
	}

	std::string identity = getIdentity();

	tcpstack.Send(cc.get(), identity+msg);

	std::string ret;
	int64 starttime=Server->getTimeMS();
	bool ok=false;
	bool herr=false;
	while(Server->getTimeMS()-starttime<=timeout)
	{
		size_t rc=cc->Read(&ret, timeout);
		if(rc==0)
		{
			if(logerr)
				ServerLogger::Log(logid, errmsg, max_loglevel);
			else
				Server->Log(errmsg, max_loglevel);

			break;
		}
		tcpstack.AddData((char*)ret.c_str(), ret.size());

		if(tcpstack.getPacket(ret))
		{
			if (conn != NULL)
			{
				conn->conn.reset(cc.release());
				conn->internet_connection = internet_connection;
			}

			return ret;
		}
	}

	if(logerr)
		ServerLogger::Log(logid, "Timeout: "+errmsg, max_loglevel);
	else
		Server->Log("Timeout: "+errmsg, max_loglevel);

	return "";
}

bool ClientMain::sendClientMessageRetry(const std::string &msg, const std::string &retok, const std::string &errmsg, unsigned int timeout, size_t retry, bool logerr, int max_loglevel, bool *retok_err, std::string* retok_str)
{
	bool res;
	do
	{
		int64 starttime=Server->getTimeMS();
		res = sendClientMessage(msg, retok, errmsg, timeout, logerr, retry>0 ? LL_DEBUG : max_loglevel, retok_err, retok_str);

		if(!res)
		{
			if(retry>0)
			{
				--retry;
				int64 passed_time=Server->getTimeMS()-starttime;

				if(passed_time<timeout)
				{
					Server->wait(static_cast<unsigned int>(timeout-passed_time));
				}
			}
			else
			{
				return res;
			}
		}
	}
	while(!res);

	return res;
}

bool ClientMain::sendClientMessage(const std::string &msg, const std::string &retok,
	const std::string &errmsg, unsigned int timeout, bool logerr, int max_loglevel, bool *retok_err,
	std::string* retok_str, SConnection* conn)
{
	CTCPStack tcpstack(internet_connection);

	std::auto_ptr<IPipe> cc;
	if (conn != NULL
		&& conn->conn.get() != NULL
		&& conn->internet_connection == internet_connection)
	{
		cc.reset(conn->conn.release());
	}
	else
	{
		cc.reset(getClientCommandConnection(NULL, 10000));
		if (cc.get() == NULL)
		{
			if (logerr)
				ServerLogger::Log(logid, "Connecting to ClientService of \"" + clientname + "\" failed: " + errmsg, max_loglevel);
			else
				Server->Log("Connecting to ClientService of \"" + clientname + "\" failed: " + errmsg, max_loglevel);

			return false;
		}
	}

	if (conn != NULL)
	{
		conn->conn.reset();
	}

	std::string identity = getIdentity();

	tcpstack.Send(cc.get(), identity+msg);

	std::string ret;
	int64 starttime=Server->getTimeMS();
	bool ok=false;
	bool herr=false;
	bool broken = false;
	while(Server->getTimeMS()-starttime<=timeout)
	{
		size_t rc=cc->Read(&ret, timeout);
		if(rc==0)
		{
			broken = true;
			break;
		}
		tcpstack.AddData((char*)ret.c_str(), ret.size());

		size_t packetsize;
		char *pck=tcpstack.getPacket(&packetsize);
		if(pck!=NULL && packetsize>0)
		{
			ret=pck;
			delete [] pck;
			if(retok_str!=NULL)
			{
				*retok_str=ret;
			}
			if(ret!=retok)
			{
				herr=true;
				if (logerr)
					ServerLogger::Log(logid, errmsg, max_loglevel);
				else
					Server->Log(errmsg, max_loglevel);

				if(retok_err!=NULL)
					*retok_err=true;

				break;
			}
			else
			{
				ok=true;
				break;
			}
		}
		else if(pck!=NULL)
		{
			delete []pck;
		}
	}
	if(!ok && !herr)
	{
		std::string reason = (broken ? "Connection broken: " : "Timeout: ");

		if(logerr)
			ServerLogger::Log(logid, reason+errmsg, max_loglevel);
		else
			Server->Log(reason +errmsg, max_loglevel);
	}

	if (conn!=NULL && (ok || herr) )
	{
		conn->conn.reset(cc.release());
		conn->internet_connection = internet_connection;
	}

	return ok;
}

void ClientMain::sendClientBackupIncrIntervall(void)
{
	SSettings* settings = server_settings->getSettings();
	int incr_freq=INT_MAX;
	if(server_settings->getUpdateFreqFileIncr()>0)
	{
		incr_freq = (std::min)(incr_freq, static_cast<int>(server_settings->getUpdateFreqFileIncr()*settings->backup_ok_mod_file));
	}
	if(server_settings->getUpdateFreqImageIncr()>0)
	{
		incr_freq = (std::min)(incr_freq, static_cast<int>(server_settings->getUpdateFreqImageIncr()*settings->backup_ok_mod_image));
	}
	if(server_settings->getUpdateFreqFileFull()>0)
	{
		incr_freq = (std::min)(incr_freq, static_cast<int>(server_settings->getUpdateFreqFileFull()*settings->backup_ok_mod_file));
	}
	if(server_settings->getUpdateFreqImageFull()>0)
	{
		incr_freq = (std::min)(incr_freq, static_cast<int>(server_settings->getUpdateFreqImageFull()*settings->backup_ok_mod_image));
	}
	int curr_startup_backup_delay = settings->startup_backup_delay;
	if(incr_freq!=INT_MAX && (incr_freq!=last_incr_freq || curr_startup_backup_delay!=last_startup_backup_delay) )
	{
		std::string delay_str;
		if(curr_startup_backup_delay>0)
		{
			delay_str="?startup_delay="+convert(curr_startup_backup_delay);
		}

		if(sendClientMessage("INCRINTERVALL \""+convert(incr_freq)+delay_str+"\"", "OK", "Sending backup interval to client failed", 10000))
		{
			last_incr_freq = incr_freq;
			last_startup_backup_delay = curr_startup_backup_delay;
		}
	}	
}

bool ClientMain::updateCapabilities(void)
{
	std::string cap=sendClientMessageRetry("CAPA", "Querying client capabilities failed", 10000, 10, false);
	if(cap!="ERR" && !cap.empty())
	{
		str_map params;
		ParseParamStrHttp(cap, &params);
		if(params["IMAGE"]!="1")
		{
			Server->Log("Client doesn't have IMAGE capability", LL_DEBUG);
			can_backup_images=false;
		}
		str_map::iterator it=params.find("FILESRV");
		if(it!=params.end())
		{
			protocol_versions.filesrv_protocol_version=watoi(it->second);
		}

		it=params.find("FILE");
		if(it!=params.end())
		{
			protocol_versions.file_protocol_version=watoi(it->second);
		}	
		it=params.find("FILE2");
		if(it!=params.end())
		{
			protocol_versions.file_protocol_version_v2=watoi(it->second);
		}
		it=params.find("SET_SETTINGS");
		if(it!=params.end())
		{
			protocol_versions.set_settings_version=watoi(it->second);
		}
		it=params.find("IMAGE_VER");
		if(it!=params.end())
		{
			protocol_versions.image_protocol_version=watoi(it->second);
		}
		it=params.find("CLIENTUPDATE");
		if(it!=params.end())
		{
			update_version=watoi(it->second);
		}
		std::string client_version_str;
		it=params.find("CLIENT_VERSION_STR");
		if(it!=params.end())
		{
			client_version_str = it->second;
			ServerStatus::setClientVersionString(clientname, it->second);
		}
		std::string os_version_str;
		it=params.find("OS_VERSION_STR");
		if(it!=params.end())
		{
			os_version_str = it->second;
			ServerStatus::setOSVersionString(clientname, it->second);
		}
		it=params.find("ALL_VOLUMES");
		if(it!=params.end())
		{
			all_volumes=(it->second);
		}
		it=params.find("ALL_NONUSB_VOLUMES");
		if(it!=params.end())
		{
			all_nonusb_volumes=(it->second);
		}
		it=params.find("ETA");
		if(it!=params.end())
		{
			protocol_versions.eta_version=watoi(it->second);
		}
		it=params.find("CDP");
		if(it!=params.end())
		{
			protocol_versions.cdp_version=watoi(it->second);
		}
		it=params.find("EFI");
		if(it!=params.end())
		{
			protocol_versions.efi_version=watoi(it->second);
		}
		it=params.find("FILE_META");
		if(it!=params.end())
		{
			protocol_versions.file_meta=watoi(it->second);
		}
		it=params.find("SELECT_SHA");
		if(it!=params.end())
		{
			protocol_versions.select_sha_version=watoi(it->second);
		}
		it = params.find("CLIENT_BITMAP");
		if (it != params.end())
		{
			protocol_versions.client_bitmap_version = watoi(it->second);
		}
		it = params.find("REQ_PREV_CBITMAP");
		if (it != params.end())
		{
			protocol_versions.require_previous_cbitmap = watoi(it->second);
		}
		it = params.find("CMD");
		if (it != params.end())
		{
			protocol_versions.cmd_version = watoi(it->second);
		}
		it = params.find("OS_SIMPLE");
		if (it != params.end())
		{
			protocol_versions.os_simple = it->second;
		}
		it = params.find("ASYNC_INDEX");
		if (it != params.end())
		{
			protocol_versions.async_index_version = watoi(it->second);
		}
		it = params.find("SYMBIT");
		if (it != params.end())
		{
			protocol_versions.symbit_version = watoi(it->second);
		}
		it = params.find("METERED");
		if (it != params.end()
			&& it->second=="1")
		{
			connection_metered = true;
		}
		it = params.find("PHASH");
		if (it != params.end())
		{
			protocol_versions.phash_version = watoi(it->second);
		}
		it = params.find("WTOKENS");
		if (it != params.end())
		{
			protocol_versions.wtokens_version = watoi(it->second);
		}
		it=params.find("RESTORE");
		if(it!=params.end())
		{
			if(it->second=="client-confirms")
			{
				ServerStatus::setRestore(clientname, ERestore_client_confirms);
			}
			else if(it->second=="server-confirms")
			{
				ServerStatus::setRestore(clientname, ERestore_server_confirms);
			}
			else
			{
				ServerStatus::setRestore(clientname, ERestore_disabled);
			}			
		}

		backup_dao->updateClientOsAndClientVersion(protocol_versions.os_simple,
			os_version_str, client_version_str, clientid);

		return true;
	}
	else
	{
		return false;
	}
}

void ClientMain::sendSettings(void)
{
	std::string s_settings;

	if(!clientsubname.empty())
	{
		s_settings+="clientsubname="+(clientsubname)+"\n";
		s_settings+="filebackup_group_offset="+convert(filebackup_group_offset)+"\n";
	}

	std::vector<std::string> settings_names=getSettingsList();
	std::vector<std::string> global_settings_names=getGlobalizedSettingsList();
	std::vector<std::string> local_settings_names=getLocalizedSettingsList();
	std::vector<std::string> only_server_settings_names=getOnlyServerClientSettingsList();

	std::auto_ptr<ISettingsReader> settings_client, settings_default, settings_global;
	server_settings->createSettingsReaders(settings_default, settings_client, settings_global);

	SSettings* settings = server_settings->getSettings();

	bool overwrite = settings->overwrite;
	bool allow_overwrite = settings->allow_overwrite;

	ServerBackupDao::CondString origSettingsData = backup_dao->getOrigClientSettings(clientid);

	std::auto_ptr<ISettingsReader> origSettings;
	if(origSettingsData.exists)
	{
		origSettings.reset(Server->createMemorySettingsReader((origSettingsData.value)));
	}

	for(size_t i=0;i<settings_names.size();++i)
	{
		std::string key=settings_names[i];
		std::string value;

		bool globalized=std::find(global_settings_names.begin(), global_settings_names.end(), key)!=global_settings_names.end();
		bool localized=std::find(local_settings_names.begin(), local_settings_names.end(), key)!=local_settings_names.end();

		if( globalized || (!overwrite && !allow_overwrite && !localized) || !settings_client->getValue(key, &value) )
		{
			if (globalized && settings_global.get()!=NULL)
			{
				if (!settings_global->getValue(key, &value))
					key = "";
			}
			else
			{
				if (!settings_default->getValue(key, &value))
					key = "";
			}
		}

		if(!key.empty())
		{
			if(!allow_overwrite)
			{
				s_settings+=(key)+"="+(value)+"\n";
			}
			else if(origSettings.get()!=NULL)
			{
				std::string orig_v;
				if( (origSettings->getValue(key, &orig_v) ||
					origSettings->getValue(key+"_def", &orig_v) ) && orig_v!=value)
				{
					s_settings += key + "_orig=" + orig_v + "\n";
				}
			}

			if(!overwrite &&
				std::find(only_server_settings_names.begin(), only_server_settings_names.end(), key)!=only_server_settings_names.end())
			{
				if (globalized && settings_global.get() != NULL)
				{
					settings_global->getValue(key, &value);
				}
				else
				{
					settings_default->getValue(key, &value);
				}
				key+="_def";
				s_settings += key + "="  + value + "\n";				
			}
			else
			{
				key+="_def";
				s_settings += key + "=" + value + "\n";
			}
		}
	}
	escapeClientMessage(s_settings);
	if(sendClientMessage("SETTINGS "+s_settings, "OK", "Sending settings to client failed", 10000))
	{
		backup_dao->insertIntoOrigClientSettings(clientid, s_settings);
	}
}	

bool ClientMain::getClientSettings(bool& doesnt_exist)
{
	doesnt_exist=false;
	std::string identity = getIdentity();

	FileClient fc(false, identity, protocol_versions.filesrv_protocol_version, internet_connection, this, use_tmpfiles?NULL:this);
	_u32 rc=getClientFilesrvConnection(&fc, server_settings);
	if(rc!=ERR_CONNECTED)
	{
		ServerLogger::Log(logid, "Getting Client settings of "+clientname+" failed - CONNECT error", LL_ERROR);
		return false;
	}
	
	IFsFile *tmp=getTemporaryFileRetry(true, std::string(), logid);
	if(tmp==NULL)
	{
		ServerLogger::Log(logid, "Error creating temporary file in BackupServerGet::getClientSettings", LL_ERROR);
		return false;
	}
	ScopedDeleteFile delete_tmp(tmp);

	std::string settings_fn = "idrivebmr/settings.cfg";

	if(!clientsubname.empty())
	{
		settings_fn = "idrivebmr/settings_"+(clientsubname)+".cfg";
	}

	rc=fc.GetFile(settings_fn, tmp, true, false, 0, false, 0);
	if(rc!=ERR_SUCCESS)
	{
		ServerLogger::Log(logid, "Error getting Client settings of "+clientname+". Errorcode: "+fc.getErrorString(rc)+" ("+convert(rc)+")", LL_ERROR);

		if(rc== ERR_CANNOT_OPEN_FILE)
		{
			doesnt_exist=true;
		}

		return false;
	}

	std::string settings_data = readToString(tmp);
	std::string tmp_fn = tmp->getFilename();
	delete_tmp.release();
	ScopedDeleteFn delete_fn(tmp_fn);
	Server->destroy(tmp);

	std::auto_ptr<ISettingsReader> sr(Server->createFileSettingsReader(tmp_fn));

	std::vector<std::string> setting_names=getSettingsList();

	bool mod=false;

	if(protocol_versions.set_settings_version>0)
	{
		std::string tmp_str;
		if(!sr->getValue("client_set_settings", &tmp_str) || tmp_str!="true" )
		{
			return true;
		}
		else
		{
			bool b=updateClientSetting("client_set_settings", "true");
			if(b)
				mod=true;

			std::string settings_update_time;
			if(sr->getValue("client_set_settings_time", &settings_update_time))
			{
				b=updateClientSetting("client_set_settings_time", settings_update_time);
				if(b)
				{
					backup_dao->insertIntoOrigClientSettings(clientid, settings_data);
					mod=true;
				}
				else
				{
					return true;
				}
			}
		}
	}

	std::vector<std::string> only_server_settings = getOnlyServerClientSettingsList();
	
	for(size_t i=0;i<setting_names.size();++i)
	{
		std::string &key=setting_names[i];
		std::string value;

		if(std::find(only_server_settings.begin(), only_server_settings.end(),
			key)!=only_server_settings.end())
		{
			continue;
		}

		if(sr->getValue(key, &value) )
		{
			if(internet_connection && key=="computername" &&
				value!=clientname)
			{
				continue;
			}

			bool b=updateClientSetting(key, value);
			if(b)
				mod=true;
		}
	}

	if(mod)
	{
		server_settings->update(true);
	}

	return true;
}

bool ClientMain::updateClientSetting(const std::string &key, const std::string &value)
{
	q_get_setting->Bind(clientid);
	q_get_setting->Bind(key);
	db_results res = q_get_setting->Read();
	q_get_setting->Reset();

	if(res.empty())
	{
		q_insert_setting->Bind(key);
		q_insert_setting->Bind(value);
		q_insert_setting->Bind(clientid);
		q_insert_setting->Write();
		q_insert_setting->Reset();
		return true;
	}
	else if(res[0]["value"]!=value)
	{
		q_update_setting->Bind(value);
		q_update_setting->Bind(key);
		q_update_setting->Bind(clientid);
		q_update_setting->Write();
		q_update_setting->Reset();
		return true;
	}
	return false;
}

void ClientMain::sendToPipe(const std::string &msg)
{
	pipe->Write(msg);
}

void ClientMain::sendClientLogdata(void)
{
	q_get_unsent_logdata->Bind(clientid);
	db_results res=q_get_unsent_logdata->Read();
	q_get_unsent_logdata->Reset();

	for(size_t i=0;i<res.size();++i)
	{
		std::string logdata=(res[i]["logdata"]);
		escapeClientMessage(logdata);
		if(sendClientMessage("2LOGDATA "+res[i]["created"]+" "+logdata, "OK", "Sending logdata to client failed", 10000, false, LL_WARNING))
		{
			q_set_logdata_sent->Bind(res[i]["id"]);
			q_set_logdata_sent->Write();
			q_set_logdata_sent->Reset();
		}
	}
}
void ClientMain::GetClientLogID(void)
{
	q_get_unsent_logdata->Bind(clientid);
	db_results res=q_get_unsent_logdata->Read();
	q_get_unsent_logdata->Reset();

	for(size_t i=0;i<res.size();++i)
	{
		virtualizationLogid.push_back(res[i]["id"]);
	}
}
MailServer ClientMain::getMailServerSettings(void)
{
	ISettingsReader *settings=Server->createDBSettingsReader(Server->getDatabase(Server->getThreadID(), IDRIVEBMRDB_SERVER), "settings_db.settings", "SELECT value FROM settings_db.settings WHERE key=? AND clientid=0");

	MailServer ms;
	ms.servername=settings->getValue("mail_servername", "");
	ms.port=(unsigned short)watoi(settings->getValue("mail_serverport", "587"));
	ms.username=settings->getValue("mail_username", "");
	ms.password=settings->getValue("mail_password", "");
	ms.mailfrom=settings->getValue("mail_from", "");
	if(ms.mailfrom.empty())
		ms.mailfrom="report@idrivebmr.example.com";

	ms.ssl_only=(settings->getValue("mail_ssl_only", "false")=="true")?true:false;
	ms.check_certificate=(settings->getValue("mail_check_certificate", "false")=="true")?true:false;

	Server->destroy(settings);
	return ms;
}

bool ClientMain::sendMailToAdmins(const std::string& subj, const std::string& message)
{
	MailServer mail_server=getMailServerSettings();
	if(mail_server.servername.empty())
		return false;

	if(url_fak==NULL)
		return false;

	ISettingsReader *settings=Server->createDBSettingsReader(Server->getDatabase(Server->getThreadID(), IDRIVEBMRDB_SERVER), "settings_db.settings", "SELECT value FROM settings_db.settings WHERE key=? AND clientid=0");
	std::string admin_addrs_str=settings->getValue("mail_admin_addrs", "");

	if(admin_addrs_str.empty())
		return false;

	std::vector<std::string> admin_addrs;
	Tokenize(admin_addrs_str, admin_addrs, ";,");

	std::string errmsg;
	bool b=url_fak->sendMail(mail_server, admin_addrs, "[IDriveBMR] "+subj, message, &errmsg);
	if(!b)
	{
		Server->Log("Sending mail failed. "+errmsg, LL_WARNING);	
		return false;
	}
	return true;
}



void ClientMain::checkClientVersion(void)
{
	std::string version;
	if (protocol_versions.os_simple == "osx")
	{
		version = getFile("idrivebmr/version_osx.txt");
	}
	else if (protocol_versions.os_simple == "linux")
	{
		version = getFile("idrivebmr/version_linux.txt");
	}
	else
	{
		version = getFile("idrivebmr/version.txt");
	}


	if(!version.empty())
	{
		std::string r=sendClientMessage("VERSION "+version, "Sending version to client failed", 10000);
		if(r=="update")
		{
			ScopedProcess process(clientname, sa_update, version, logid, false);

			std::string signature_file = "idrivebmr/IDriveBMRUpdate.sig2";
			std::string installer_file = "idrivebmr/IDriveBMRUpdate.exe";

			if (protocol_versions.os_simple == "osx")
			{
				signature_file = "idrivebmr/IDriveBMRUpdateMac.sig2";
				installer_file = "idrivebmr/IDriveBMRUpdateMac.sh";
			}
			else if (protocol_versions.os_simple == "linux")
			{
				signature_file = "idrivebmr/IDriveBMRUpdateLinux.sig2";
				installer_file = "idrivebmr/IDriveBMRUpdateLinux.sh";
			}
			else if (update_version < 2)
			{
				signature_file = "idrivebmr/IDriveBMRUpdate.sig";
			}


			std::auto_ptr<IFile> sigfile(Server->openFile(signature_file, MODE_READ));
			if(sigfile.get()==NULL)
			{
				ServerLogger::Log(logid, "Error opening sigfile", LL_ERROR);
				return;
			}
			std::auto_ptr<IFile> updatefile(Server->openFile(installer_file, MODE_READ));
			if(updatefile.get()==NULL)
			{
				ServerLogger::Log(logid, "Error opening updatefile", LL_ERROR);
				return;
			}			
			size_t datasize=3*sizeof(_u32)+version.size()+(size_t)sigfile->Size()+(size_t)updatefile->Size();

			CTCPStack tcpstack(internet_connection);
			std::auto_ptr<IPipe> cc(getClientCommandConnection(server_settings, 10000));
			if(cc.get()==NULL)
			{
				ServerLogger::Log(logid, "Connecting to ClientService of \""+clientname+"\" failed - CONNECT error", LL_ERROR);
				return;
			}

			std::string msg="1CLIENTUPDATE size="+convert(datasize)+"&silent_update="+convert(server_settings->getSettings()->silent_update);

			std::string identity = getIdentity();
			tcpstack.Send(cc.get(), identity+msg);

			int timeout=5*60*1000;

			_u32 c_size=little_endian((_u32)version.size());
			if(!cc->Write((char*)&c_size, sizeof(c_size), timeout) )
			{
				return;
			}
			if(!cc->Write(version, timeout) )
			{
				return;
			}
			c_size= little_endian((_u32)sigfile->Size());
			if(!cc->Write((char*)&c_size, sizeof(c_size), timeout) )
			{
				return;
			}
			if(!sendFile(cc.get(), sigfile.get(), timeout) )
			{
				return;
			}
			c_size= little_endian((_u32)updatefile->Size());
			if(!cc->Write((char*)&c_size, sizeof(c_size), timeout) )
			{
				return;
			}
			if(!sendFile(cc.get(), updatefile.get(), timeout) )
			{
				return;
			}

			sigfile.reset();
			updatefile.reset();

			std::string ret;
			int64 starttime=Server->getTimeMS();
			bool ok=false;
			while(Server->getTimeMS()-starttime<=5*60*1000)
			{
				size_t rc=cc->Read(&ret, timeout);
				if(rc==0)
				{
					ServerLogger::Log(logid, "Reading from client failed in update", LL_ERROR);
					break;
				}
				tcpstack.AddData((char*)ret.c_str(), ret.size());

				std::string msg;
				if(tcpstack.getPacket(msg))
				{
					if(msg =="ok")
					{
						ok=true;
						break;
					}
					else
					{
						ok=false;
						ServerLogger::Log(logid, "Error in update: "+ msg, LL_ERROR);
						break;
					}
				}
			}

			if(!ok)
			{
				ServerLogger::Log(logid, "Timeout: In client update of \"" + clientname + "\"", LL_ERROR);
			}

			cc.reset();

			client_updated_time = Server->getTimeMS();

			if(ok)
			{
				ServerLogger::Log(logid, "Updated client \""+clientname+"\" successfully", LL_INFO);
			}

			updateCapabilities();
		}
	}
}

bool ClientMain::sendFile(IPipe *cc, IFile *f, int timeout)
{
	char buf[4096];
	_u32 r;
	while((r=f->Read(buf, 4096))>0)
	{
		if(!cc->Write(buf, r, timeout))
			return false;
	}
	return true;
}

sockaddr_in ClientMain::getClientaddr(void)
{
	IScopedLock lock(clientaddr_mutex);
	return clientaddr;
}


bool ClientMain::isBackupsRunningOkay(bool file, bool incr)
{
	IScopedLock lock(running_backup_mutex);
	if(running_backups<server_settings->getSettings()->max_sim_backups)
	{
		if (running_backups_allowed
			&& incr)
		{
			++running_backups;
			if (file)
			{
				++running_file_backups;
			}
		}
		return running_backups_allowed;
	}
	else
	{
		return false;
	}
}

void ClientMain::stopBackupRunning(bool file)
{
	IScopedLock lock(running_backup_mutex);
	if (running_backups == 0)
	{
		Server->Log("running_backups is zero", LL_ERROR);
		assert(false);
		return;
	}
	--running_backups;
	if(file)
	{
		if (running_file_backups == 0)
		{
			Server->Log("running_file_backups is zero", LL_ERROR);
			assert(false);
			return;
		}
		--running_file_backups;
	}
}

int ClientMain::getNumberOfRunningBackups(void)
{
	IScopedLock lock(running_backup_mutex);
	return running_backups;
}

int ClientMain::getNumberOfRunningFileBackups(void)
{
	IScopedLock lock(running_backup_mutex);
	return running_file_backups;
}

IPipeThrottler *ClientMain::getThrottler(int speed_bps)
{
	IScopedLock lock(throttle_mutex);

	if(client_throttler==NULL)
	{
		client_throttler=Server->createPipeThrottler(new ThrottleUpdater(clientid,
			internet_connection?ThrottleScope_Internet:ThrottleScope_Local));
	}
	else
	{
		bool percent_max;
		size_t bps = BackupServer::throttleSpeedToBps(speed_bps, percent_max);
		client_throttler->changeThrottleLimit(bps,
			percent_max);
	}

	return client_throttler;
}

void ClientMain::updateClientAccessKey()
{
	std::string access_key = ServerSettings::generateRandomAuthKey(32);
	backup_dao->updateOrInsertSetting(clientid, "client_access_key", access_key);
	ServerSettings::updateClient(clientid);

	sendClientMessageRetry("CLIENT ACCESS KEY key=" + access_key + "&token=" + server_token, "OK",
		"Error sending client access key", 10000, 10, false);
}

bool ClientMain::isDataplanOkay(bool file)
{
	return isDataplanOkay(server_settings, file);
}

bool ClientMain::isOnline(ServerChannelThread& channel_thread)
{
	return ServerStatus::getStatus(clientname).r_online || channel_thread.isOnline();
}

bool ClientMain::isDataplanOkay(ServerSettings* local_settings, bool file)
{
	if (!internet_connection)
	{
		return true;
	}

	int64 limit;
	std::string pattern;
	std::string hostname;
	if (!connection_metered)
	{
		unsigned int addr = ServerStatus::getStatus(clientname).ip_addr;
		unsigned char *ips = reinterpret_cast<unsigned char*>(&addr);
		std::string	ip = convert(ips[0]) + "." + convert(ips[1]) + "." + convert(ips[2]) + "." + convert(ips[3]);

		hostname = Server->LookupHostname(ip);
		if (hostname.empty())
		{
			return true;
		}

		if (!DataplanDb::getInstance()->getLimit(hostname, pattern, limit))
		{
			limit = 1LL * 1024 * 1024 * 1024 * 1024;
		}
	}
	else
	{
		limit = 1LL * 1024 * 1024 * 1024;
		pattern = "*info from client*";
		hostname = "*not retrieved*";
	}

	if (local_settings == NULL)
	{
		return true;
	}

	int64 appl_limit;
	if (file)
	{
		appl_limit = local_settings->getSettings()->internet_file_dataplan_limit;
	}
	else
	{
		appl_limit = local_settings->getSettings()->internet_image_dataplan_limit;
	}

	if (limit<0 || limit >= appl_limit)
	{
		return true;
	}

	ServerLogger::Log(logid, std::string("Not running ") + (file ? "file" : "image") 
		+ " backup because currently estimated data plan of "
		+ PrettyPrintBytes(limit) + " per month "
		"with pattern \""+pattern+"\" for hostname \""+hostname
		+"\" is smaller than configured limit of "
		+ PrettyPrintBytes(appl_limit) + " per month", LL_DEBUG);

	return false;
}

void ClientMain::setConnectionMetered(bool b)
{
	connection_metered = b;
}

void ClientMain::forceReauthenticate()
{
	do_reauthenticate = true;
}

bool ClientMain::inBackupWindow(Backup * backup)
{
	if (backup->isFileBackup())
	{
		if (backup->isIncrementalBackup())
		{
			return ServerSettings::isInTimeSpan(server_settings->getBackupWindowIncrFile());
		}
		else
		{
			return ServerSettings::isInTimeSpan(server_settings->getBackupWindowFullFile());
		}
	}
	else
	{
		if (backup->isIncrementalBackup())
		{
			return ServerSettings::isInTimeSpan(server_settings->getBackupWindowIncrImage());
		}
		else
		{
			return ServerSettings::isInTimeSpan(server_settings->getBackupWindowFullImage());
		}
	}
}

IPipe *ClientMain::getClientCommandConnection(ServerSettings* server_settings, int timeoutms, std::string* clientaddr)
{
	std::string curr_clientname = (clientname);
	if(!clientsubname.empty())
	{
		curr_clientname =  (clientmainname);
	}

	if(clientaddr!=NULL)
	{
		unsigned int ip = ServerStatus::getStatus((curr_clientname)).ip_addr;
		unsigned char *ips=reinterpret_cast<unsigned char*>(&ip);
		*clientaddr=convert(ips[0])+"."+convert(ips[1])+"."+convert(ips[2])+"."+convert(ips[3]);
	}
	if(internet_connection)
	{
		IPipe *ret=InternetServiceConnector::getConnection(curr_clientname, SERVICE_COMMANDS, timeoutms);
		if(server_settings!=NULL && ret!=NULL)
		{
			int internet_speed=server_settings->getInternetSpeed();
			if(internet_speed!=0
				&& internet_speed!=-1)
			{
				ret->addThrottler(getThrottler(internet_speed));
			}
			int global_internet_speed=server_settings->getGlobalInternetSpeed();
			if(global_internet_speed!=0
				&& global_internet_speed!=-1)
			{
				ret->addThrottler(BackupServer::getGlobalInternetThrottler(global_internet_speed));
			}
		}
		return ret;
	}
	else
	{
		IPipe *ret=Server->ConnectStream(inet_ntoa(getClientaddr().sin_addr), serviceport, timeoutms);
		if(server_settings!=NULL && ret!=NULL)
		{
			int local_speed=server_settings->getLocalSpeed();
			if(local_speed!=0
				&& local_speed!=-1)
			{
				ret->addThrottler(getThrottler(local_speed));
			}
			int global_local_speed=server_settings->getGlobalLocalSpeed();
			if(global_local_speed!=0
				&& global_local_speed!=-1)
			{
				ret->addThrottler(BackupServer::getGlobalLocalThrottler(global_local_speed));
			}
		}
		return ret;
	}
}

_u32 ClientMain::getClientFilesrvConnection(FileClient *fc, ServerSettings* server_settings, int timeoutms)
{
	std::string curr_clientname = (clientname);
	if(!clientsubname.empty())
	{
		curr_clientname =  (clientmainname);
	}

	fc->setProgressLogCallback(this);
	if(internet_connection)
	{
		IPipe *cp=InternetServiceConnector::getConnection(curr_clientname, SERVICE_FILESRV, timeoutms);

		_u32 ret=fc->Connect(cp);

		if(server_settings!=NULL)
		{
			int internet_speed=server_settings->getInternetSpeed();
			if(internet_speed!=0
				&& internet_speed!=-1)
			{
				fc->addThrottler(getThrottler(internet_speed));
			}
			int global_internet_speed=server_settings->getGlobalInternetSpeed();
			if(global_internet_speed!=0
				&& global_internet_speed!=-1)
			{
				fc->addThrottler(BackupServer::getGlobalInternetThrottler(global_internet_speed));
			}
		}

		fc->setReconnectionTimeout(c_internet_fileclient_timeout);

		return ret;
	}
	else
	{
		sockaddr_in addr=getClientaddr();
		_u32 ret=fc->Connect(&addr);

		if(server_settings!=NULL)
		{
			int local_speed=server_settings->getLocalSpeed();
			if(local_speed!=0
				&& local_speed!=-1)
			{
				fc->addThrottler(getThrottler(local_speed));
			}
			int global_local_speed=server_settings->getGlobalLocalSpeed();
			if(global_local_speed!=0
				&& global_local_speed!=-1)
			{
				fc->addThrottler(BackupServer::getGlobalLocalThrottler(global_local_speed));
			}
		}

		return ret;
	}
}

bool ClientMain::getClientChunkedFilesrvConnection(std::auto_ptr<FileClientChunked>& fc_chunked, ServerSettings* server_settings, int timeoutms)
{
	std::string curr_clientname = (clientname);
	if(!clientsubname.empty())
	{
		curr_clientname =  (clientmainname);
	}

	std::string identity = getIdentity();
	if(internet_connection)
	{
		IPipe *cp=InternetServiceConnector::getConnection(curr_clientname, SERVICE_FILESRV, timeoutms);
		if(cp!=NULL)
		{
			fc_chunked.reset(new FileClientChunked(cp, false, &tcpstack, this, use_tmpfiles?NULL:this, identity, NULL));
			fc_chunked->setReconnectionTimeout(c_internet_fileclient_timeout);
		}
		else
		{
			return false;
		}
	}
	else
	{
		sockaddr_in addr=getClientaddr();
		IPipe *pipe=Server->ConnectStream(inet_ntoa(getClientaddr().sin_addr), TCP_PORT, timeoutms);
		if(pipe!=NULL)
		{
			fc_chunked.reset(new FileClientChunked(pipe, false, &tcpstack, this, use_tmpfiles?NULL:this, identity, NULL));
		}
		else
		{
			return false;
		}
	}

	fc_chunked->setProgressLogCallback(this);

	if(fc_chunked->getPipe()!=NULL && server_settings!=NULL)
	{
		int speed;
		if(internet_connection)
		{
			speed=server_settings->getInternetSpeed();
		}
		else
		{
			speed=server_settings->getLocalSpeed();
		}
		if(speed!=0
			&& speed!=-1)
		{
			fc_chunked->addThrottler(getThrottler(speed));
		}

		if(internet_connection)
		{
			int global_speed=server_settings->getGlobalInternetSpeed();
			if(global_speed!=0
				&& global_speed!=-1)
			{
				fc_chunked->addThrottler(BackupServer::getGlobalInternetThrottler(global_speed));
			}
		}
		else
		{
			int global_speed=server_settings->getGlobalLocalSpeed();
			if(global_speed!=0
				&& global_speed!=-1)
			{
				fc_chunked->addThrottler(BackupServer::getGlobalLocalThrottler(global_speed));
			}
		}
	}

	return true;
}

IFsFile *ClientMain::getTemporaryFileRetry(bool use_tmpfiles, const std::string& tmpfile_path, logid_t logid)
{
	int tries=50;
	IFsFile *pfd=NULL;
	while(pfd==NULL)
	{
		if(use_tmpfiles)
		{
			pfd=Server->openTemporaryFile();
		}
		else
		{
			size_t num;
			{
				IScopedLock lock(tmpfile_mutex);
				num=tmpfile_num++;
			}
			std::string fn = tmpfile_path + os_file_sep() + convert(num);
			Server->deleteFile(os_file_prefix(fn));
			pfd=Server->openFile(os_file_prefix(fn), MODE_RW_CREATE);
		}

		if(pfd==NULL)
		{
			if(!os_directory_exists(os_file_prefix(tmpfile_path)))
			{
				ServerLogger::Log(logid, "Temporary file path did not exist. Creating it.", LL_INFO);

				if(!os_create_dir_recursive(os_file_prefix(tmpfile_path)))
				{
					ServerLogger::Log(logid, "Error creating temporary file path. "+os_last_error_str(), LL_WARNING);
				}
			}
			ServerLogger::Log(logid, "Error opening temporary file. Retrying...", LL_WARNING);
			--tries;
			if(tries<0)
			{
				return NULL;
			}
			Server->wait(1000);
		}
	}
	return pfd;
}

void ClientMain::destroyTemporaryFile(IFile *tmp)
{
	std::string fn=tmp->getFilename();
	Server->destroy(tmp);
	Server->deleteFile(fn);
}

IPipe * ClientMain::new_fileclient_connection(void)
{
	std::string curr_clientname = (clientname);
	if(!clientsubname.empty())
	{
		curr_clientname =  (clientmainname);
	}

	IPipe *rp=NULL;
	if(internet_connection)
	{
		rp=InternetServiceConnector::getConnection(curr_clientname, SERVICE_FILESRV, c_filesrv_connect_timeout);
	}
	else
	{
		sockaddr_in addr=getClientaddr();
		rp=Server->ConnectStream(inet_ntoa(getClientaddr().sin_addr), TCP_PORT, c_filesrv_connect_timeout);
	}
	return rp;
}

bool ClientMain::handle_not_enough_space(const std::string &path)
{
	int64 free_space=-1;
	if(!path.empty())
	{
		free_space = os_free_space(os_file_prefix(path));
		if(free_space==-1)
		{
			free_space = os_free_space(os_file_prefix(ExtractFilePath(path)));
		}
	}
	if(free_space==-1)
	{
		free_space=os_free_space(os_file_prefix(server_settings->getSettings()->backupfolder));
	}
	if(free_space!=-1 && free_space<minfreespace_min)
	{
		Server->Log("No free space in backup folder. Free space="+PrettyPrintBytes(free_space)+" MinFreeSpace="+PrettyPrintBytes(minfreespace_min), LL_WARNING);

		if(!ServerCleanupThread::cleanupSpace(minfreespace_min) )
		{
			ServerLogger::Log(logid, "FATAL: Could not free space. NOT ENOUGH FREE SPACE.", LL_ERROR);
			sendMailToAdmins("Fatal error occurred during backup", ServerLogger::getWarningLevelTextLogdata(logid));
			return false;
		}
	}

	return true;
}

unsigned int ClientMain::exponentialBackoffTime( size_t count, unsigned int sleeptime, unsigned div )
{
	return static_cast<unsigned int>((std::max)(static_cast<double>(sleeptime), static_cast<double>(sleeptime)*pow(static_cast<double>(div), static_cast<double>(count))));
}

bool ClientMain::exponentialBackoff(size_t count, int64 lasttime, unsigned int sleeptime, unsigned div)
{
	if(count>0)
	{
		int64 passed_time=Server->getTimeMS()-lasttime;
		int64 sleeptime_exp = 1LL*exponentialBackoffTime(count, sleeptime, div)*1000;

		return passed_time>=sleeptime_exp;
	}
	return true;
}


unsigned int ClientMain::exponentialBackoffTimeImage()
{
	return exponentialBackoffTime(count_image_backup_try, c_sleeptime_failed_imagebackup, c_exponential_backoff_div);
}

unsigned int ClientMain::exponentialBackoffTimeFile()
{
	return exponentialBackoffTime(count_file_backup_try, c_sleeptime_failed_filebackup, c_exponential_backoff_div);
}

bool ClientMain::exponentialBackoffImage()
{
	bool res = exponentialBackoff(count_image_backup_try, last_image_backup_try, c_sleeptime_failed_imagebackup, c_exponential_backoff_div);
	Server->Log("exponentialBackoff " + convert(res));
	return res;
}

bool ClientMain::exponentialBackoffFile()
{
	return exponentialBackoff(count_file_backup_try, last_file_backup_try, c_sleeptime_failed_filebackup, c_exponential_backoff_div);
}

bool ClientMain::exponentialBackoffCdp()
{
	return exponentialBackoff(count_cdp_backup_try, last_cdp_backup_try, c_sleeptime_failed_filebackup, c_exponential_backoff_div);
}

bool ClientMain::pauseRetryBackup()
{
	bool res = Server->getTimeMS() - last_backup_try >= 5 * 60 * 1000;
	return res;
}

bool ClientMain::sendServerIdentity(bool retry_exit)
{
	{
		IScopedLock lock(clientaddr_mutex);
		session_identity.clear();
	}
	bool c = true;
	while (c)
	{
		c = false;
		bool retok_err = false;
		std::string ret_str;
		bool b = sendClientMessage("ADD IDENTITY", "OK", "Sending Identity to client \"" + clientname + "\" failed. Retrying soon...", 10000, false, LL_DEBUG, &retok_err, &ret_str);
		if (!b)
		{
			if (retok_err)
			{
				if (ret_str != "needs certificate")
				{
					ServerStatus::setStatusError(clientname, se_ident_error);
				}
				else
				{
					ServerStatus::setStatusError(clientname, se_none);
					needs_authentification = true;
					return true;
				}
			}

			if (!retry_exit)
			{
				return false;
			}

			unsigned int retry_time = ident_err_retry_time;

			if (retok_err)
			{
				retry_time = ident_err_retry_time_retok;
			}

			if (retok_err
				&& !ret_str.empty())
			{
				Server->Log("Sending Identity to client \"" + clientname + "\" failed. Retrying in "+PrettyPrintTime(retry_time)+"... (returned \"" + ret_str + "\")", LL_INFO);
			}
			else
			{
				Server->Log("Sending Identity to client \"" + clientname + "\" failed. Retrying in " + PrettyPrintTime(retry_time) + "...", LL_INFO);
			}

			c = true;
			std::string msg;
			pipe->Read(&msg, retry_time);
			if (msg == "exit" || msg == "exitnow")
			{
				pipe->Write(msg);
				return false;
			}
			else if (next(msg, 0, "address"))
			{
				updateClientAddress(msg.substr(7));

				tcpstack.setAddChecksum(internet_connection);
			}
			else
			{
				pipe->Write(msg);
				Server->wait(retry_time);
			}
		}
		else
		{
			ServerStatus::setStatusError(clientname, se_none);
		}
	}

	return true;
}

bool ClientMain::authenticatePubKey()
{
	if(crypto_fak==NULL)
	{
		return false;
	}

	std::string params;

	if (!clientsubname.empty())
	{
		params = " clientsubname=" + EscapeParamString(clientsubname);
	}

	std::string challenge = sendClientMessageRetry("GET CHALLENGE" + params, "Failed to get challenge from client", 10000, 10, false, LL_INFO);

	if(challenge=="ERR")
	{
		return false;
	}

	if(!challenge.empty())
	{
		std::string signature;
		std::string signature_ecdsa409k1;
		std::string privkey = getFile("idrivebmr/server_ident.priv");

		if(privkey.empty())
		{
			Server->Log("Cannot read private key idrivebmr/server_ident.priv", LL_ERROR);
			return false;
		}

		std::string privkey_ecdsa409k1 = getFile("idrivebmr/server_ident_ecdsa409k1.priv");

		if(privkey_ecdsa409k1.empty())
		{
			Server->Log("Cannot read private key idrivebmr/server_ident_ecdsa409k1.priv", LL_ERROR);
			return false;
		}

		bool rc = crypto_fak->signDataDSA(privkey, challenge, signature);

		if(!rc)
		{
			Server->Log("Signing challenge failed", LL_ERROR);
			return false;
		}

		rc = crypto_fak->signData(privkey_ecdsa409k1, challenge, signature_ecdsa409k1);

		if(!rc)
		{
			Server->Log("Signing challenge failed -2", LL_ERROR);
			return false;
		}

		std::string pubkey = getFile("idrivebmr/server_ident.pub");

		if(pubkey.empty())
		{
			Server->Log("Reading public key from idrivebmr/server_ident.pub failed", LL_ERROR);
			return false;
		}

		std::string pubkey_ecdsa = getFile("idrivebmr/server_ident_ecdsa409k1.pub");

		if(pubkey.empty())
		{
			Server->Log("Reading public key from idrivebmr/server_ident_ecdsa409k1.pub failed", LL_ERROR);
			return false;
		}

		std::string identity = ServerSettings::generateRandomAuthKey(20);

		bool ret = sendClientMessageRetry("SIGNATURE#pubkey="+base64_encode_dash(pubkey)+
			"&pubkey_ecdsa409k1="+base64_encode_dash(pubkey_ecdsa)+
			"&signature="+base64_encode_dash(signature)+
			"&signature_ecdsa409k1="+base64_encode_dash(signature_ecdsa409k1)+
			"&session_identity="+identity +
			(clientsubname.empty() ? "" : "&clientsubname="+clientsubname), "ok", "Error sending server signature to client", 10000, 10, true);

		if(ret)
		{
			IScopedLock lock(clientaddr_mutex);
			session_identity = "#I"+identity+"#";
			session_identity_refreshtime = Server->getTimeMS();
		}

		return ret;
	}
	else
	{
		return false;
	}
}

void ClientMain::timeoutRestores()
{
	IScopedLock lock(restore_mutex.get());

	for (size_t i = 0; i < running_restores.size();)
	{
		if (Server->getTimeMS() - running_restores[i].last_active > 5 * 60 * 1000) //5min
		{
			ServerLogger::Log(running_restores[i].log_id, "Restore was inactive for 5min. Timeout. Stopping restore...", LL_ERROR);

			finishFailedRestore(running_restores[i].restore_identity, running_restores[i].log_id, running_restores[i].status_id, running_restores[i].restore_id);

			running_restores.erase(running_restores.begin() + i);
		}
		else
		{
			++i;
		}
	}
}

std::string ClientMain::getIdentity()
{
	IScopedLock lock(clientaddr_mutex);
	return session_identity.empty() ? server_identity : session_identity;
}

bool ClientMain::run_script( std::string name, const std::string& params, logid_t logid)
{
#ifdef _WIN32
	name = name + ".bat";
#endif
	if(!FileExists(name))
	{
		ServerLogger::Log(logid, "Script does not exist "+name, LL_DEBUG);
		return true;
	}

#ifdef _WIN32
	//I ... waaa ... even ... :(
	std::string quoted_script_name = greplace(" ", "\" \"", name);
#else
	std::string quoted_script_name = "\"" + greplace("\"", "\\\"", name) + "\"";
#endif

	quoted_script_name +=" "+params;

	quoted_script_name +=" 2>&1";

#ifdef _WIN32
	FILE* fp = _wpopen(Server->ConvertToWchar(quoted_script_name).c_str(), L"rb");
#else
	FILE* fp = popen(quoted_script_name.c_str(), "r");
#endif

	if(!fp)
	{
		ServerLogger::Log(logid, "Could not open pipe for command "+name, LL_DEBUG);
		return false;
	}

	std::string output;
	while(!feof(fp) && !ferror(fp))
	{
		char buf[4097];
		size_t r = fread(buf, 1, 4096, fp);
		buf[r]=0;
		output+=buf;
	}

#ifdef _WIN32
	int rc = _pclose(fp);
#else
	int rc = pclose(fp);
#endif

	if(rc!=0)
	{
		ServerLogger::Log(logid, "Script "+name+" had error (code "+convert(rc)+")", LL_ERROR);
	}

	std::vector<std::string> toks;
	Tokenize(output, toks, "\n");

	for(size_t i=0;i<toks.size();++i)
	{
		ServerLogger::Log(logid, "Script output Line("+convert(i+1)+"): " + toks[i], rc!=0?LL_ERROR:LL_INFO);
	}

	return rc == 0;
}

void ClientMain::log_progress( const std::string& fn, int64 total, int64 downloaded, int64 speed_bps )
{
	std::string fn_wo_token = fn;
	std::string share = getuntil("/", fn);
	if (!share.empty())
	{
		if (share.find("|") != std::string::npos)
		{
			fn_wo_token = getafter("|", fn);
		}
	}

	if(total>0 && total!=LLONG_MAX)
	{
		int pc_complete = 0;
		if(total>0)
		{
			pc_complete = static_cast<int>((static_cast<float>(downloaded)/total)*100.f);
		}
		ServerLogger::Log(logid, "Loading \""+ fn_wo_token +"\". "+convert(pc_complete)+"% finished "+PrettyPrintBytes(downloaded)+"/"+PrettyPrintBytes(total)+" at "+PrettyPrintSpeed(static_cast<size_t>(speed_bps)), LL_DEBUG);
	}
	else
	{
		ServerLogger::Log(logid, "Loading \""+ fn_wo_token +"\". Loaded "+PrettyPrintBytes(downloaded)+" at "+PrettyPrintSpeed(static_cast<size_t>(speed_bps)), LL_DEBUG);
	}
}



void ClientMain::updateClientAddress(const std::string& address_data)
{
	IScopedLock lock(clientaddr_mutex);
	memcpy(&clientaddr, address_data.data(), sizeof(sockaddr_in));
	bool prev_internet_connection = internet_connection;
	internet_connection = (address_data[sizeof(sockaddr_in)] == 0) ? false : true;

	if (prev_internet_connection != internet_connection)
	{
		IScopedLock lock(throttle_mutex);

		if (client_throttler != NULL)
		{
			client_throttler->changeThrottleUpdater(new
				ThrottleUpdater(clientid, internet_connection ?
					ThrottleScope_Internet : ThrottleScope_Local));
		}
	}
}

bool ClientMain::createDirectoryForClient()
{
	std::string backupfolder=server_settings->getSettings()->backupfolder;
	if(!os_create_dir(os_file_prefix(backupfolder+os_file_sep()+clientname)) && !os_directory_exists(os_file_prefix(backupfolder+os_file_sep()+clientname)) )
	{
		Server->Log("Could not create or read directory for client \""+clientname+"\". "+os_last_error_str(), LL_ERROR);
		return false;
	}
	return true;
}

bool ClientMain::isRunningImageBackup(const std::string& letter)
{
	for(size_t i=0;i<backup_queue.size();++i)
	{
		if(!backup_queue[i].backup->isFileBackup()
			&& backup_queue[i].letter== normalizeVolumeUpper(letter) )
		{
			return true;
		}
	}

	return false;
}

std::string ClientMain::normalizeVolumeUpper(std::string volume)
{
	if (volume.size() == 1)
	{
		strupper(&volume);
		return volume + ":";
	}

	if (volume.size() == 2
		&& volume[1] == ':')
	{
		strupper(&volume);
		return volume;
	}

	if (volume.size() == 3
		&& volume[1] == ':'
		&& (volume[2] == '\\' || volume[2] == '/'))
	{
		strupper(&volume);
		return volume.substr(0, 2);
	}

	return volume;
}

bool ClientMain::isImageGroupQueued(const std::string & letter, bool full)
{
	std::string image_snapshot_groups = server_settings->getSettings()->image_snapshot_groups;
	std::vector<std::string> groups;
	Tokenize(image_snapshot_groups, groups, "|");

	image_snapshot_groups = strlower(image_snapshot_groups);

	std::string vol = normalizeVolumeUpper(letter);

	for (size_t i = 0; i < groups.size(); ++i)
	{
		std::vector<std::string> vols;
		Tokenize(groups[i], vols, ";,");

		for (size_t j = 0; j < vols.size(); ++j)
		{
			vols[j] = normalizeVolumeUpper(vols[j]);
		}

		if (image_snapshot_groups=="all"
			|| std::find(vols.begin(), vols.end(), vol) != vols.end())
		{
			for (size_t k = 0; k < backup_queue.size(); ++k)
			{
				if (backup_queue[k].backup->isIncrementalBackup()
					&& full)
				{
					continue;
				}

				if (image_snapshot_groups == "all"
					|| std::find(vols.begin(), vols.end(), normalizeVolumeUpper(backup_queue[k].letter))
						!= vols.end())
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool ClientMain::isRunningFileBackup(int group, bool queue_only)
{
	for(size_t i=0;i<backup_queue.size();++i)
	{
		if(backup_queue[i].backup->isFileBackup() && backup_queue[i].group==group)
		{
			if (queue_only)
			{
				return true;
			}
			else if (backup_queue[i].ticket != ILLEGAL_THREADPOOL_TICKET)
			{
				return true;
			}
		}
	}

	return false;
}

void ClientMain::addContinuousChanges( const std::string& changes )
{
	IScopedLock lock(continuous_mutex);
	if(continuous_backup!=NULL)
	{
		continuous_backup->addChanges(changes);
	}
}

void ClientMain::setContinuousBackup( BackupServerContinuous* cb )
{
	IScopedLock lock(continuous_mutex);

	if(continuous_backup!=NULL)
	{
		continuous_backup->doStop();
	}

	continuous_backup = cb;
}

void ClientMain::addShareToCleanup( int clientid, const SShareCleanup& cleanupData )
{
	if(fileserv==NULL) return;

	IScopedLock lock(cleanup_mutex);
	std::vector<SShareCleanup>& tocleanup = cleanup_shares[clientid];
	tocleanup.push_back(cleanupData);
}

void ClientMain::cleanupRestoreShare(int clientid, std::string restore_identity)
{
	IScopedLock lock(cleanup_mutex);

	std::vector<SShareCleanup>& tocleanup = cleanup_shares[clientid];

	for (size_t i = 0; i < tocleanup.size();)
	{
		if (tocleanup[i].identity == restore_identity)
		{
			cleanupShare(tocleanup[i]);
			tocleanup.erase(tocleanup.begin() + i);
		}
		else
		{
			++i;
		}
	}
}

bool ClientMain::finishRestore(int64 restore_id)
{
	IScopedLock lock(restore_mutex.get());

	for (size_t i = 0; i < running_restores.size(); ++i)
	{
		if (running_restores[i].restore_id == restore_id)
		{
			running_restores.erase(running_restores.begin() + i);
			return true;
		}
	}

	return false;
}

bool ClientMain::updateRestoreRunning(int64 restore_id)
{
	IScopedLock lock(restore_mutex.get());

	for (size_t i = 0; i < running_restores.size(); ++i)
	{
		if (running_restores[i].restore_id == restore_id)
		{
			running_restores[i].last_active = Server->getTimeMS();
			return true;
		}
	}

	return false;
}

void ClientMain::cleanupShares()
{
	if(fileserv!=NULL)
	{
		IScopedLock lock(cleanup_mutex);
		std::vector<SShareCleanup>& tocleanup = cleanup_shares[clientid];

		for(size_t i=0;i<tocleanup.size();++i)
		{
			cleanupShare(tocleanup[i]);
		}

		tocleanup.clear();
	}
}

void ClientMain::cleanupShare(SShareCleanup & tocleanup)
{
	if (tocleanup.cleanup_file)
	{
		std::string cleanupfile = fileserv->getShareDir(tocleanup.name, tocleanup.identity);
		if (!cleanupfile.empty())
		{
			Server->deleteFile(cleanupfile);
		}
	}

	if (tocleanup.remove_callback)
	{
		fileserv->removeMetadataCallback(tocleanup.name, tocleanup.identity);
	}

	fileserv->removeDir(tocleanup.name, tocleanup.identity);

	if (!tocleanup.identity.empty())
	{
		fileserv->removeIdentity(tocleanup.identity);
	}
}

void ClientMain::finishFailedRestore(std::string restore_identity, logid_t log_id, int64 status_id, int64 restore_id)
{
	ServerStatus::stopProcess(clientname, status_id);

	int errors = 0;
	int warnings = 0;
	int infos = 0;
	std::string logdata = ServerLogger::getLogdata(log_id, errors, warnings, infos);

	backup_dao->saveBackupLog(clientid, errors, warnings, infos, 0,
		0, 0, 1);

	backup_dao->saveBackupLogData(db->getLastInsertID(), logdata);

	backup_dao->setRestoreDone(0, restore_id);

	IScopedLock lock(cleanup_mutex);

	std::vector<SShareCleanup>& tocleanup = cleanup_shares[clientid];

	for (size_t i = 0; i < tocleanup.size();)
	{
		if (tocleanup[i].identity == restore_identity)
		{
			cleanupShare(tocleanup[i]);
			tocleanup.erase(tocleanup.begin() + i);
		}
		else
		{
			++i;
		}
	}
}

bool ClientMain::startBackupBarrier( int64 timeout_seconds )
{
	IScopedLock lock(running_backup_mutex);

	running_backups_allowed=false;

	int64 starttime = Server->getTimeMS();

	while(running_backups>0 && Server->getTimeMS()-starttime<timeout_seconds*1000)
	{
		lock.relock(NULL);
		Server->wait(60000);
		lock.relock(running_backup_mutex);
	}

	if(running_backups==0)
	{
		return true;
	}

	running_backups_allowed=true;
	return false;
}

void ClientMain::stopBackupBarrier()
{
	IScopedLock lock(running_backup_mutex);
	running_backups_allowed=true;
}

void ClientMain::updateCapa()
{
	update_capa = true;
}

std::vector<SLogEntry> ClientMain::parseLogData(int64 max_duration, const std::string & data)
{
	std::vector<SLogEntry> ret;
	std::vector<std::string> lines;
	Tokenize(data, lines, "\n");
	int64 initial_time = Server->getTimeSeconds();
	for (size_t i = 0; i<lines.size(); ++i)
	{
		size_t s1 = lines[i].find("-");
		size_t s2 = lines[i].find("-", s1 + 1);

		if (s1 == std::string::npos)
		{
			continue;
		}

		SLogEntry entry;

		entry.loglevel = atoi(lines[i].substr(0, s1).c_str());

		entry.time = 0;

		if (s2 != std::string::npos)
		{
			entry.time = os_atoi64(lines[i].substr(s1 + 1, s2 - s1));

			if (i == 0)
			{
				initial_time = entry.time;
				continue;
			}

			entry.time = initial_time - entry.time;

			if (entry.time>max_duration + 60)
			{
				entry.time = 0;
			}

			entry.data = lines[i].substr(s2 + 1);
		}
		else
		{
			entry.data = lines[i].substr(s1 + 1);
		}

		ret.push_back(entry);
	}

	return ret;
}

bool ClientMain::authenticateIfNeeded(bool retry_exit, bool force)
{
	{
		IScopedLock lock(clientaddr_mutex);

		if (session_identity_refreshtime!=0
			&& Server->getTimeMS() - session_identity_refreshtime<30 * 60 * 1000
			&& !force)
		{
			return true;
		}

		session_identity.clear();
	}

	bool c = false;
	do
	{			
		c=false;

		bool b = authenticatePubKey();
		if(!b && needs_authentification)
		{
			ServerStatus::setStatusError(clientname, se_authentication_error);

			if (!retry_exit)
			{
				Server->Log("Authentification failed for client \"" + clientname + "\"", LL_INFO);
				return false;
			}

			Server->wait(5*60*1000); //5min

			std::string msg;
			pipe->Read(&msg, ident_err_retry_time);
			if(msg=="exit" || msg=="exitnow")
			{
				Server->Log("client_main Thread for client \""+clientname+"\" finished and the authentification failed", LL_INFO);
				pipe->Write(msg);
				return false;
			}
			else if (next(msg, 0, "address"))
			{
				updateClientAddress(msg.substr(7));

				tcpstack.setAddChecksum(internet_connection);
			}
			else
			{
				pipe->Write(msg);
				Server->wait(ident_err_retry_time);
				sendServerIdentity(false);
			}

			c=true;
		}
		else
		{
			ServerStatus::setStatusError(clientname, se_none);
		}
	}
	while(c);

	return true;
}


bool ClientMain::UpdateCloudVirtualization(int backupId)
{
	allBackupsInDatabase.push_back(backupId);
	return true;
}

bool ClientMain::PreviousVirtualizeVerificationComplete()
{
	//check the virtStatus in virtualizationStatus column
	//get the timestamp from it
	//if timestamp is greater than 30 mins and virtStatus=1 reset that to 0, return true
	//else do not start the backup, return false
	//
	unsigned int duration = 30*60; //30 mins
	int64 time_now = Server->getTimeSeconds();
	std::string virtStatus = backup_dao->getVirtualizationStatus(clientid);

	Json::Reader reader;
	Json::Value root;

	reader.parse(virtStatus, root);
	uint64 virtStartTime = root["VirtStartTime"].asUInt64();
	int virtstatus = root["VirtStatus"].asInt();
        Json::Value bkpid = root["backupIds"];
	Json::Value logids = root["logids"];

	uint64 elapsedtime = time_now - virtStartTime;
	//if elapsedTime is greater than 30 min and previous virtualization is still pending then
	//the previous backup is marked as failed and logs table is updated with proper message
	if(elapsedtime >= duration && virtstatus == VIRT_PENDING)
	{
		Server->Log("Virtualization Timeout: Setting previous backup as failure", LL_WARNING);
		for(int i = 0; i < bkpid.size(); i++){
			backup_dao->setImageBackupIncomplete(bkpid[i].asInt());
		}
		std::string logmsg;
		for(int i = 0; i < logids.size(); i++){
			int id = stoi(logids[i].asString());
			logmsg = "2-" + std::to_string(time_now) + "-Virtualization Timeout";
			backup_dao->appendLogData(id, logmsg);
			//increment errors column in logs table
			backup_dao->incrementErrors(id);
		}

		//Setting virtualization status failed
		root["VirtStatus"] = VIRT_FAILED;
		Json::FastWriter fastWriter;
		std::string output = fastWriter.write(root);
		backup_dao->setVirtualizationStatus(clientid, output);
		return true;
	}
	return false;
}
