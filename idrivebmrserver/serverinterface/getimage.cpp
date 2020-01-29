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

#ifndef CLIENT_ONLY

#include "action_header.h"

ACTION_IMPL(getimage)
{
	Helper helper(tid, &POST, &PARAMS);

	JSON::Object ret;
	SUser *session=helper.getSession();
	if(session!=NULL && session->id==SESSION_ID_INVALID) return;
	if(session!=NULL )
	{
		int img_id=watoi(POST["image_id"]);
		std::map<std::string, IObject* >::iterator iter=session->mCustom.find("image_"+convert(img_id));
		if(iter!=session->mCustom.end())
		{
			Server->setContentType(tid, "image/png");
			IFile *img=(IFile*)iter->second;
			std::string fdata;
			fdata=img->Read((_u32)img->Size());
			Server->Write(tid, fdata, false);
			std::string fn=img->getFilename();
			Server->destroy(img);
			Server->deleteFile(fn);
			session->mCustom.erase(iter);
			return;
		}
		else
		{
			ret.set("error", 2);
		}
	}	
	else
	{
		ret.set("error", JSON::Value(1));
	}
	
    helper.Write(ret.stringify(false));
}

#endif //CLIENT_ONLY
