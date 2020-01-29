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

#include "server_dir_links.h"
#include "../idrivebmrcommon/os_functions.h"
#include "../Interface/Server.h"
#include "../stringtools.h"
#include "server_settings.h"
#include "../Interface/Mutex.h"
#include "../Interface/Database.h"
#include "../Interface/File.h"
#include "database.h"
#include <assert.h>

namespace
{
	void reference_all_sublinks(ServerLinkDao& link_dao, int clientid, const std::string& target, const std::string& new_target)
	{
		std::string escaped_target = escape_glob_sql(target);

		std::vector<ServerLinkDao::DirectoryLinkEntry> entries = link_dao.getLinksInDirectory(clientid, escaped_target+os_file_sep()+"*");

		for(size_t i=0;i<entries.size();++i)
		{
			std::string subpath = entries[i].target.substr(target.size());
			std::string new_link_path = new_target + subpath;
			link_dao.addDirectoryLink(clientid, entries[i].name, new_link_path);
		}
	}

	bool reference_parents(ServerLinkDao& link_dao, int clientid, const std::string& target,
		const std::string& new_pool_name, int depth, bool remove)
	{
		bool ret = true;
		std::string path = target;
		std::string sub_path;
		for (int i = 0; i < depth; ++i)
		{
			sub_path = os_file_sep() + ExtractFileName(path, os_file_sep()) + sub_path;
			path = ExtractFilePath(path, os_file_sep());

			if (os_is_symlink(os_file_prefix(path)))
			{
				std::string link_src_dir;
				if (!os_get_symlink_target(os_file_prefix(path), link_src_dir))
				{
					Server->Log("Could not get symlink target of source directory \"" + path + "\". (1)", LL_ERROR);
					ret = false;
					continue;
				}

				std::string pool_name = ExtractFileName(link_src_dir);

				if (pool_name.empty())
				{
					Server->Log("Error extracting pool name from link source \"" + link_src_dir + "\" (1)", LL_ERROR);
					ret = false;
					continue;
				}

				std::vector<ServerLinkDao::DirectoryLinkEntry> entries = link_dao.getLinksByPoolName(clientid, pool_name);

				for (size_t j = 0; j < entries.size(); ++j)
				{
					std::string new_link_path = entries[j].target + sub_path;

					if (new_link_path == target)
						continue;

					assert(os_directory_exists(new_link_path));
					if (remove)
					{
						link_dao.removeDirectoryLinkWithName(clientid, new_link_path, new_pool_name);
					}
					else
					{
						link_dao.addDirectoryLink(clientid, new_pool_name, new_link_path);
					}
				}
			}
		}

		return ret;
	}

	IMutex* dir_link_mutex;
	std::map<int, IMutex*> dir_link_client_mutexes;
}

void dir_link_lock_client_mutex(int clientid, IScopedLock& lock)
{
	IScopedLock lock2(dir_link_mutex);

	std::map<int, IMutex*>::iterator it = dir_link_client_mutexes.find(clientid);
	if (it != dir_link_client_mutexes.end())
	{
		IMutex* mut = it->second;
		lock2.relock(NULL);
		lock.relock(mut);
	}
	else
	{
		IMutex* mut = Server->createMutex();
		dir_link_client_mutexes.insert(std::make_pair(clientid, mut));
		lock.relock(mut);
	}
}

std::string escape_glob_sql(const std::string& glob)
{
	std::string ret;
	ret.reserve(glob.size());
	for(size_t i=0;i<glob.size();++i)
	{
		if(glob[i]=='?')
		{
			ret+="[?]";
		}
		else if(glob[i]=='[')
		{
			ret+="[[]";
		}
		else if(glob[i]=='*')
		{
			ret+="[*]";
		}
		else
		{
			ret+=glob[i];
		}
	}
	return ret;
}


bool link_directory_pool( int clientid, const std::string& target_dir, const std::string& src_dir, const std::string& pooldir, bool with_transaction,
	ServerLinkDao*& link_dao, ServerLinkJournalDao*& link_journal_dao, int depth)
{
	if (link_dao == NULL)
	{
		link_dao=new ServerLinkDao(Server->getDatabase(Server->getThreadID(), IDRIVEBMRDB_SERVER_LINKS));
	}

	DBScopedSynchronous synchonous_link_journal(NULL);

	if (!with_transaction && link_journal_dao == NULL)
	{
		IDatabase* link_journal_db = Server->getDatabase(Server->getThreadID(), IDRIVEBMRDB_SERVER_LINK_JOURNAL);
		link_journal_dao = new ServerLinkJournalDao(link_journal_db);
		synchonous_link_journal.reset(link_journal_db);
	}

	DBScopedSynchronous synchonous_link(link_dao->getDatabase());

	IScopedLock lock(NULL);
	dir_link_lock_client_mutex(clientid, lock);

	DBScopedWriteTransaction link_transaction(link_dao->getDatabase());

	std::string link_src_dir;
	std::string pool_name;
	bool remove_glob=false;
	if(os_is_symlink(os_file_prefix(src_dir)))
	{
		if(!os_get_symlink_target(os_file_prefix(src_dir), link_src_dir))
		{
			Server->Log("Could not get symlink target of source directory \""+src_dir+"\".", LL_ERROR);
			return false;
		}

		pool_name = ExtractFileName(link_src_dir);

		if(pool_name.empty())
		{
			Server->Log("Error extracting pool name from link source \""+link_src_dir+"\"", LL_ERROR);
			return false;
		}

		link_dao->addDirectoryLink(clientid, pool_name, target_dir);
		reference_all_sublinks(*link_dao, clientid, src_dir, target_dir);
		remove_glob=true;
	}
	else if(os_directory_exists(os_file_prefix(src_dir)))
	{
		std::string parent_src_dir;
		do 
		{
			pool_name = ServerSettings::generateRandomAuthKey(10)+convert(Server->getTimeSeconds())+convert(Server->getTimeMS());
			parent_src_dir = pooldir + os_file_sep() + pool_name.substr(0, 2);
			link_src_dir = parent_src_dir + os_file_sep() + pool_name;
		} while (os_directory_exists(os_file_prefix(link_src_dir)));

		if(!os_directory_exists(os_file_prefix(parent_src_dir)) && !os_create_dir_recursive(os_file_prefix(parent_src_dir)))
		{
			Server->Log("Could not create directory for pool directory: \""+parent_src_dir+"\"", LL_ERROR);
			return false;
		}

		std::vector<int64> parent_ids;
		reference_parents(*link_dao, clientid, src_dir, pool_name, depth, false);
		link_dao->addDirectoryLink(clientid, pool_name, src_dir);
		reference_all_sublinks(*link_dao, clientid, src_dir, target_dir);
		remove_glob = true;
		link_dao->addDirectoryLink(clientid, pool_name, target_dir);
				

		int64 replay_entry_id;
		if(!with_transaction)
		{
			link_journal_dao->addDirectoryLinkJournalEntry(src_dir, link_src_dir);
			replay_entry_id = link_journal_dao->getLastId();
		}

		link_transaction.end();

		void* transaction=NULL;
		if(with_transaction)
		{
			transaction=os_start_transaction();

			if(!transaction)
			{
				Server->Log("Error starting filesystem transaction", LL_ERROR);
				reference_parents(*link_dao, clientid, src_dir, pool_name, depth, true);
				link_dao->removeDirectoryLink(clientid, src_dir);
				link_dao->removeDirectoryLinkGlob(clientid, escape_glob_sql(target_dir) + os_file_sep() + "*");
				link_dao->removeDirectoryLink(clientid, target_dir);
				return false;
			}
		}

		if(!os_rename_file(os_file_prefix(src_dir), os_file_prefix(link_src_dir), transaction))
		{
			Server->Log("Could not rename folder \""+src_dir+"\" to \""+link_src_dir+"\"", LL_ERROR);
			os_finish_transaction(transaction);
			reference_parents(*link_dao, clientid, src_dir, pool_name, depth, true);
			link_dao->removeDirectoryLink(clientid, src_dir);
			link_dao->removeDirectoryLinkGlob(clientid, escape_glob_sql(target_dir) + os_file_sep() + "*");
			link_dao->removeDirectoryLink(clientid, target_dir);
			
			if (!with_transaction)
			{
				link_journal_dao->removeDirectoryLinkJournalEntry(replay_entry_id);
			}

			return false;
		}

		bool l_isdir = true;
		if(!os_link_symbolic(os_file_prefix(link_src_dir), os_file_prefix(src_dir), transaction, &l_isdir))
		{
			Server->Log("Could not create a symbolic link at \""+src_dir+"\" to \""+link_src_dir+"\"", LL_ERROR);
			os_rename_file(link_src_dir, src_dir, transaction);
			os_finish_transaction(transaction);
			reference_parents(*link_dao, clientid, src_dir, pool_name, depth, true);
			link_dao->removeDirectoryLink(clientid, src_dir);
			link_dao->removeDirectoryLinkGlob(clientid, escape_glob_sql(target_dir) + os_file_sep() + "*");
			link_dao->removeDirectoryLink(clientid, target_dir);

			if (!with_transaction)
			{
				link_journal_dao->removeDirectoryLinkJournalEntry(replay_entry_id);
			}

			return false;
		}

		if(with_transaction)
		{
			if(!os_finish_transaction(transaction))
			{
				Server->Log("Error finishing filesystem transaction", LL_ERROR);
				reference_parents(*link_dao, clientid, src_dir, pool_name, depth, true);
				link_dao->removeDirectoryLink(clientid, src_dir);
				link_dao->removeDirectoryLinkGlob(clientid, escape_glob_sql(target_dir) + os_file_sep() + "*");
				link_dao->removeDirectoryLink(clientid, target_dir);
				return false;
			}
		}
		else
		{
			link_journal_dao->removeDirectoryLinkJournalEntry(replay_entry_id);
		}
	}
	else
	{
		Server->Log("Cannot link directory \"" + target_dir + "\" because source directory \"" + src_dir + "\" does not exist.", LL_DEBUG);
		return false;
	}

	if(!os_link_symbolic(os_file_prefix(link_src_dir), os_file_prefix(target_dir)))
	{
		Server->Log("Error creating symbolic link from \"" + link_src_dir +"\" to \"" +
			target_dir+"\" -2", LL_ERROR);

		link_dao->removeDirectoryLink(clientid, target_dir);

		if(remove_glob)
		{
			link_dao->removeDirectoryLinkGlob(clientid, escape_glob_sql(target_dir)+os_file_sep()+"*");
		}

		return false;
	}

	return true;
}

bool replay_directory_link_journal( )
{
	IScopedLock lock(dir_link_mutex);

	ServerLinkJournalDao link_journal_dao(Server->getDatabase(Server->getThreadID(), IDRIVEBMRDB_SERVER_LINK_JOURNAL));

	std::vector<ServerLinkJournalDao::JournalEntry> journal_entries = link_journal_dao.getDirectoryLinkJournalEntries();

	bool has_error=false;

	for(size_t i=0;i<journal_entries.size();++i)
	{
		const ServerLinkJournalDao::JournalEntry& je = journal_entries[i];

		std::string symlink_real_target;

		if(!os_is_symlink(je.linkname)
			|| (os_get_symlink_target(je.linkname, symlink_real_target) && symlink_real_target!=je.linktarget) )
		{
			if(os_directory_exists(je.linktarget))
			{
				os_remove_symlink_dir(os_file_prefix(je.linkname));
				if(!os_link_symbolic(os_file_prefix(je.linktarget), os_file_prefix(je.linkname)))
				{
					Server->Log("Error replaying symlink journal: Could not create link at \""+je.linkname+"\" to \""+je.linktarget+"\". "+os_last_error_str(), LL_ERROR);
					has_error=true;
				}
			}
		}
	}

	link_journal_dao.removeDirectoryLinkJournalEntries();

	return has_error;
}

bool remove_directory_link(const std::string & path, ServerLinkDao & link_dao, int clientid,
	std::auto_ptr<DBScopedSynchronous>& synchronous_link_dao, bool with_transaction)
{
	std::string pool_path;
	if (!os_get_symlink_target(path, pool_path))
	{
		Server->Log("Error getting symlink path in pool of \"" + path + "\"", LL_ERROR);
		return false;
	}

	std::string pool_name = ExtractFileName(pool_path, os_file_sep());

	if (pool_name.empty())
	{
		Server->Log("Error extracting pool name from pool path \"" + pool_path + "\"", LL_ERROR);
		return false;
	}

	std::string directory_pool = ExtractFileName(ExtractFilePath(ExtractFilePath(pool_path, os_file_sep()), os_file_sep()), os_file_sep());

	if (directory_pool != ".directory_pool")
	{
		//Other symlink. Simply delete

		if (!os_remove_symlink_dir(os_file_prefix(path)))
		{
			Server->Log("Error removing symlink dir \"" + path + "\"", LL_ERROR);
		}
		return true;
	}

	std::string target_raw;
	if (next(path, 0, os_file_prefix("")))
	{
		target_raw = path.substr(os_file_prefix("").size());
	}
	else
	{
		target_raw = path;
	}

	if (with_transaction)
	{
		synchronous_link_dao.reset(new DBScopedSynchronous(link_dao.getDatabase()));
		link_dao.getDatabase()->BeginWriteTransaction();
	}

	link_dao.removeDirectoryLink(clientid, target_raw);

	if (link_dao.getLastChanges()>0)
	{
		bool ret = true;
		if (link_dao.getDirectoryRefcount(clientid, pool_name) == 0)
		{
			ret = remove_directory_link_dir(path, link_dao, clientid, false, false);
			ret = ret && os_remove_dir(os_file_prefix(pool_path));

			if (!ret)
			{
				Server->Log("Error removing directory link \"" + path + "\" with pool path \"" + pool_path + "\"", LL_ERROR);
			}
		}
		else
		{
			link_dao.removeDirectoryLinkGlob(clientid, escape_glob_sql(target_raw) + os_file_sep() + "*");
		}
	}
	else
	{
		Server->Log("Directory link \"" + path + "\" with pool path \"" + pool_path + "\" not found in database. Deleting symlink only.", LL_WARNING);
	}


	if (!os_remove_symlink_dir(os_file_prefix(path)))
	{
		Server->Log("Error removing symlink dir \"" + path + "\"", LL_ERROR);
	}

	{
		std::auto_ptr<IFile> dir_f(Server->openFile(os_file_prefix(ExtractFilePath(path, os_file_sep())), MODE_READ_SEQUENTIAL_BACKUP));
		if (dir_f.get() != NULL)
		{
			dir_f->Sync();
		}
	}

	if (with_transaction)
	{
		link_dao.getDatabase()->EndTransaction();;
		synchronous_link_dao.reset();
	}

	return true;
}

namespace
{
	struct SSymlinkCallbackData
	{
		SSymlinkCallbackData(ServerLinkDao* link_dao,
			int clientid, bool with_transaction)
			: link_dao(link_dao), clientid(clientid),
			with_transaction(with_transaction)
		{

		}

		ServerLinkDao* link_dao;
		std::auto_ptr<DBScopedSynchronous> synchronous_link_dao;
		int clientid;
		bool with_transaction;
	};

	bool symlink_callback(const std::string &path, bool* isdir, void* userdata)
	{
		if (isdir!=NULL && !*isdir)
		{
			if (!Server->deleteFile(os_file_prefix(path)))
			{
				Server->Log("Error removing symlink file \"" + path + "\"", LL_ERROR);
			}
			return true;
		}

		SSymlinkCallbackData* data = reinterpret_cast<SSymlinkCallbackData*>(userdata);

		return remove_directory_link(path, *data->link_dao, data->clientid,
			data->synchronous_link_dao, data->with_transaction);
	}
}

bool remove_directory_link_dir(const std::string &path, ServerLinkDao& link_dao, int clientid, bool delete_root, bool with_transaction)
{
	IScopedLock lock(NULL);
	dir_link_lock_client_mutex(clientid, lock);

	SSymlinkCallbackData userdata(&link_dao, clientid, with_transaction);
	return os_remove_nonempty_dir(os_file_prefix(path), symlink_callback, &userdata, delete_root);
}

bool reference_contained_directory_links(ServerLinkDao& link_dao, int clientid, 
	const std::string& pool_name, const std::string &path, const std::string& link_path)
{
	std::vector<SFile> files = getFiles(link_path + path);

	std::vector<ServerLinkDao::DirectoryLinkEntry> targets = link_dao.getLinksByPoolName(clientid, pool_name);

	bool ret = true;
	for (size_t i = 0; i < files.size(); ++i)
	{
		if (files[i].issym)
		{
			std::string sympath = link_path + path + os_file_sep() + files[i].name;
			std::string pool_path;
			if (!os_get_symlink_target(sympath, pool_path))
			{
				Server->Log("Error getting symlink path in pool of \"" + link_path + path + "\" (contains)", LL_ERROR);
				continue;
			}

			std::string curr_pool_name = ExtractFileName(pool_path, os_file_sep());

			if (curr_pool_name.empty())
			{
				continue;
			}

			std::string directory_pool = ExtractFileName(ExtractFilePath(ExtractFilePath(pool_path, os_file_sep()), os_file_sep()), os_file_sep());

			if (directory_pool != ".directory_pool")
			{
				continue;
			}

			for (size_t j = 0; j < targets.size(); ++j)
			{
				std::string ref_target = targets[j].target + path + os_file_sep() + files[i].name;
				if (link_dao.getDirectoryRefcountWithTarget(clientid, curr_pool_name, ref_target) == 0)
				{
					Server->Log("Adding missing directory reference of pool " + curr_pool_name + " to \"" + ref_target + "\"", LL_WARNING);
					link_dao.addDirectoryLink(clientid, curr_pool_name, ref_target);
				}
			}

			if (!reference_contained_directory_links(link_dao, clientid,
				curr_pool_name, std::string(), pool_path))
			{
				ret = false;
			}
		}
		else if (files[i].isdir)
		{
			if (!reference_contained_directory_links(link_dao, clientid,
				pool_name, path + os_file_sep() + files[i].name, link_path))
			{
				ret = false;
			}
		}
	}

	return ret;
}

bool is_directory_link(const std::string & path)
{
	int ftype = os_get_file_type(os_file_prefix(path));

	if ( (ftype & EFileType_Directory) == 0
		|| (ftype & EFileType_Symlink) == 0)
	{
		return false;
	}

	std::string symlink_target;
	if (!os_get_symlink_target(os_file_prefix(path), symlink_target))
	{
		return false;
	}

	std::string directory_pool = ExtractFileName(ExtractFilePath(ExtractFilePath(symlink_target, os_file_sep()), os_file_sep()), os_file_sep());

	return directory_pool == ".directory_pool";
}

void init_dir_link_mutex()
{
	dir_link_mutex=Server->createMutex();
}

void destroy_dir_link_mutex()
{
	Server->destroy(dir_link_mutex);
}
