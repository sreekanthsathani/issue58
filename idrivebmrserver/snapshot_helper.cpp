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

#include "snapshot_helper.h"
#include "../Interface/Server.h"
#include <stdlib.h>
#include "../stringtools.h"
#include "server.h"
#include "../idrivebmrcommon/os_functions.h"
#ifdef _WIN32
#define WEXITSTATUS(x) x
#else
#include <sys/types.h>
#include <sys/wait.h>
#endif

std::string SnapshotHelper::helper_name="idrivebmr_snapshot_helper";

int SnapshotHelper::isAvailable(void)
{
	int rc=system((helper_name+" test").c_str());

	rc = WEXITSTATUS(rc);
	if (rc >= 10)
	{
		return rc - 10;
	}

	return -1;
}

bool SnapshotHelper::createEmptyFilesystem(bool image, std::string clientname, std::string name, std::string& errmsg)
{
	int rc=os_popen((helper_name + " " + convert(BackupServer::getSnapshotMethod(image)) + " create \""+(clientname)+"\" \""+(name)+"\" 2>&1").c_str(), errmsg);
	return rc==0;
}

bool SnapshotHelper::snapshotFileSystem(bool image, std::string clientname, std::string old_name, std::string snapshot_name, std::string& errmsg)
{
	int rc=os_popen((helper_name + " " + convert(BackupServer::getSnapshotMethod(image)) + " snapshot \""+(clientname)+"\" \""+(old_name)+"\" \""+(snapshot_name)+"\" 2>&1").c_str(), errmsg);
	return rc==0;
}

bool SnapshotHelper::removeFilesystem(bool image, std::string clientname, std::string name)
{
	int rc=system((helper_name + " " + convert(BackupServer::getSnapshotMethod(image)) + " remove \""+(clientname)+"\" \""+(name)+"\"").c_str());
	return rc==0;
}

bool SnapshotHelper::isSubvolume(bool image, std::string clientname, std::string name)
{
	int rc=system((helper_name + " "+convert(BackupServer::getSnapshotMethod(image))+" issubvolume \""+(clientname)+"\" \""+(name)+"\"").c_str());
	return rc==0;
}

void SnapshotHelper::setSnapshotHelperCommand(std::string helper_command)
{
	helper_name=helper_command;
}

bool SnapshotHelper::makeReadonly(bool image, std::string clientname, std::string name)
{
	int rc = system((helper_name + " " + convert(BackupServer::getSnapshotMethod(image)) + " makereadonly \"" + clientname + "\" \"" + name + "\"").c_str());
	return rc == 0;
}

std::string SnapshotHelper::getMountpoint(bool image, std::string clientname, std::string name)
{
	std::string ret;
	int rc = os_popen(helper_name + " " + convert(BackupServer::getSnapshotMethod(image)) + " mountpoint \"" + clientname + "\" \"" + name + "\"",
		ret);

	if (rc != 0)
	{
		return std::string();
	}

	return trim(ret);
}
