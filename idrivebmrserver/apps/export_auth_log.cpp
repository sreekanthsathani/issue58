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

#include "app.h"
#include <fstream>
#include "../serverinterface/login.h"
#include "../../stringtools.h"
#include "../../idrivebmrcommon/os_functions.h"

int export_auth_log()
{
	open_server_database(true);
	open_settings_database();

	IDatabase *db=Server->getDatabase(Server->getThreadID(), IDRIVEBMRDB_SERVER);
	if(db==NULL)
	{
		Server->Log("Could not open main database", LL_ERROR);
		return 1;
	}

	db_results res = db->Read("SELECT strftime('%Y-%m-%d %H:%M', logintime, 'localtime') AS iso_logintime, username, ip, method FROM settings_db.login_access_log ORDER BY logintime DESC");

	Server->deleteFile("idrivebmr/auth_log.csv");
	std::fstream out("idrivebmr/auth_log.csv", std::ios::out|std::ios::binary);
	if(!out.is_open())
	{
		Server->Log("Error opening \""+Server->getServerWorkingDir()+os_file_sep()+"idrivebmr/auth_log.csv\" for writing", LL_ERROR);
		return 1;
	}

	for(size_t i=0;i<res.size();++i)
	{
		LoginMethod loginMethod = static_cast<LoginMethod>(watoi(res[i]["method"]));
		out << res[i]["iso_logintime"] << ";"
			<< res[i]["username"] << ";"
			<< res[i]["ip"] << ";";

		switch(loginMethod)
		{
		case LoginMethod_Webinterface:
			out << "webinterface"; break;
		case LoginMethod_RestoreCD:
			out << "restorecd"; break;
		}

		out << "\r\n";
	}

	out.close();
	Server->Log("Auth log has been written to \""+Server->getServerWorkingDir()+os_file_sep()+"idrivebmr/auth_log.csv\"", LL_INFO);


	return 0;
}