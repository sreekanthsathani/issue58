#pragma once

#include "../Interface/File.h"
#include "../idrivebmrcommon/os_functions.h"
#include "file_metadata.h"

void writeFileRepeat(IFile *f, const std::string &str);

std::string escapeListName( const std::string& listname );

void writeFileItem(IFile* f, SFile cf, size_t* written=NULL, size_t* change_identicator_off=NULL);
void writeFileItem(IFile* f, SFile cf, std::string extra);


class FileListParser
{
public:
	FileListParser();

	void reset(void);

	bool nextEntry(char ch, SFile &data, std::map<std::string, std::string>* extra);

private:

	enum ParseState
	{
		ParseState_Type,
		ParseState_TypeFinish,
		ParseState_Quote,
		ParseState_Name,
		ParseState_NameEscape,
		ParseState_NameFinish,
		ParseState_Filesize,
		ParseState_ModifiedTime,
		ParseState_ExtraParams
	};

	ParseState state;
	std::string t_name;
	int64 pos;
};
