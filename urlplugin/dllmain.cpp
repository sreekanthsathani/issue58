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

#include "../vld.h"
#ifdef _WIN32
#define DLLEXPORT extern "C" __declspec (dllexport)
#else
#define DLLEXPORT extern "C"
#endif


#ifndef STATIC_PLUGIN
#define DEF_SERVER
#endif

#include "../Interface/Server.h"

#ifndef STATIC_PLUGIN
IServer *Server;
#else
#include "../StaticPluginRegistration.h"

extern IServer* Server;

#define LoadActions LoadActions_urlplugin
#define UnloadActions UnloadActions_urlplugin
#endif

#include "../Interface/Action.h"
#include "../stringtools.h"

#include "pluginmgr.h"
#include "IUrlFactory.h"

CUrlPluginMgr *urlpluginmgr=NULL;


DLLEXPORT void LoadActions(IServer* pServer)
{
	Server=pServer;

	urlpluginmgr=new CUrlPluginMgr;

	Server->RegisterPluginThreadsafeModel( urlpluginmgr, "url");

#ifndef STATIC_PLUGIN
	Server->Log("Loaded -url- plugin", LL_INFO);
#endif
}

DLLEXPORT void UnloadActions(void)
{
}

#ifdef STATIC_PLUGIN
namespace
{
	static RegisterPluginHelper register_plugin(LoadActions, UnloadActions, 0);
}
#endif