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

#include "HTTPAction.h"

#include "../Interface/Server.h"
#include "../Interface/Pipe.h"
#include "../Interface/OutputStream.h"

#include "../stringtools.h"

#include <stdexcept>

namespace
{
	class PipeOutputStream : public IOutputStream
	{
	public:
		PipeOutputStream(IPipe* pipe)
			: _pipe(pipe)
		{
		}

		virtual void write(const std::string &tw)
		{
			write(tw.c_str(), tw.size(), STDOUT);
		}

		virtual void write(const char* buf, size_t count, ostream_type_t stream)
		{
			if(count==0)
				return;

			if(!_pipe->Write(buf, count))
			{
				Server->Log("Send failed in PipeOutputStream", LL_INFO);
				throw std::runtime_error("Send failed in PipeOutputStream");
			}
		}

	private:
		IPipe* _pipe;
	};
}

CHTTPAction::CHTTPAction(const std::string &pName, const std::string pContext, const std::string &pGETStr, const std::string pPOSTStr, const str_map &pRawPARAMS, IPipe *pOutput)
{
	name=pName;
	GETStr=pGETStr;
	POSTStr=pPOSTStr;
	RawPARAMS=pRawPARAMS;
	output=pOutput;
	context=pContext;
}

#define MAP(x,y) { std::map<std::string, std::string>::iterator iter=RawPARAMS.find(x); if(iter!=RawPARAMS.end() ) PARAMS.insert(std::pair<std::string, std::string>(y, iter->second) ); }

void CHTTPAction::operator()(void)
{
	std::map<std::string, std::string> GET;
	std::map<std::string, std::string> POST;
	ParseParamStrHttp(GETStr, &GET, true);
	ParseParamStrHttp(POSTStr, &POST, true);

	std::map<std::string, std::string> PARAMS;

	MAP("POSTFILEKEY","POSTFILEKEY");
	MAP("ACCEPT-LANGUAGE", "ACCEPT_LANGUAGE");
	MAP("REMOTE_ADDR", "REMOTE_ADDR");
	MAP("X-FORWARDED-FOR", "HTTP_X_FORWARDED_FOR")

	PipeOutputStream pipe_output_stream(output);

	THREAD_ID tid=0;
	try
	{
		pipe_output_stream.write("HTTP/1.1 200 ok\r\nCache-Control: max-age=0\r\n");
		tid = Server->Execute(name, context, GET, POST, PARAMS, &pipe_output_stream);
	}
	catch(...)
	{
		return;
	}

	if( tid==0 )
	{
		std::string error="Error: Unknown action ["+EscapeHTML(name)+"]";
		Server->Log(error, LL_WARNING);
		output->Write("Content-type: text/html; charset=UTF-8\r\n\r\n"+error);
	}
}