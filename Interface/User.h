#ifndef IUSER_H
#define IUSER_H

#include <map>
#include <string>

#include "Object.h"
#include "Types.h"

struct SUser
{
	std::string username;
	std::string session;
	std::string ident_data;
	int id;
	str_map mStr;
	int_map mInt;
	float_map mFloat;
	std::map<std::string, IObject* > mCustom;
	int64 lastused;

	IObject* getCustomPtr(std::string str)
	{
		std::map<std::string, IObject* >::iterator iter=mCustom.find(str);
		if( iter!=mCustom.end() )
		{
			return iter->second;
		}
		else
			return NULL;
	}

	//private
	void *mutex;
	void *lock;
};

#endif //IUSER_H