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

#include "vld.h"

#include <string>
#include <vector>
#include <fstream>
#include <sstream> 
#include <map>
#include <stdlib.h>

#ifndef _WIN32
#include <memory.h>
#include <stdlib.h>
#else
#include <Windows.h>
#endif

#include "utf8/utf8.h"
#include "Interface/Types.h"
#include "stringtools.h"


using namespace std;

typedef int s32;
typedef unsigned int u32;
typedef float f32;

//--------------------------------------------------------------------
/**
*	liefert einen teil des strings nach dem gelieferten teilstring
*/
string getafterinc(const std::string &str,const std::string &data)
{
	size_t pos=data.find(str);
	if(pos!=std::string::npos)
	{
		return data.substr(pos);
	}
	else
	{
		return "";
	}
}

string getafter(const std::string &str,const std::string &data)
{
	std::string ret=getafterinc(str,data);
	ret.erase(0,str.size() );
	return ret;
}

//--------------------------------------------------------------------
/**
*	liefert einen teil des strings zwischen zwei teilstrings
*/
string getbetween(string s1,string s2,string data)
{
	size_t off1=data.find(s1);

	if(off1==-1)return "";

	off1+=s1.size();

	size_t off2=data.find(s2,off1);

	if(s2=="\n")
	{
		size_t off3=data.find("\r\n",off1);
		if(off3<off2)
			off2=off3;
	}

	if(off2==-1)return "";

	string ret=data.substr(off1,off2-off1);
	return ret;
}

//--------------------------------------------------------------------
/**
*	lösche einen teil des strings ab einem bestimmten teilstring
*/
string strdelete(string str,string data)
{
	size_t off=data.find(str);
	if(off==-1)
		return data;
	data.erase(off,str.size() );
	return data;
}

//--------------------------------------------------------------------
/**
*	string in eine datei schreiben (hinzufügen)
*/
void writestring(string str,string file)
{
	fstream out;
	out.open(file.c_str(),ios::out|ios::binary);

	out.write(str.c_str(),(s32)str.size() );

	out.close();
}

void writestring(char *str, unsigned int len,std::string file)
{
        fstream out;
	out.open(file.c_str(),ios::out|ios::binary);

	out.write(str,len );
        out.flush();

	out.close();
}

//--------------------------------------------------------------------
/**
*	liefert einen teil des strings bevor einem teilstring
*/
string getuntil(string str,string data)
{
	size_t off=data.find(str);
	if(off==-1)
		return "";
	return data.substr(0,off);
}

//--------------------------------------------------------------------
/**
*	liefert einen teil des strings bevor einem teilstring inklusive
*	dem teilstring..
*/
string getuntilinc(string str,string data)
{
	size_t off=data.find(str);
	if(off==-1)
		return "";
	return data.substr(0,off+str.size());
}

//--------------------------------------------------------------------
/**
*	eine bestimmte zeile (getrennt duch \n) aus dem string anfordern
*/
string getline(s32 line,const string &str)
{
	s32 num=0;
	string tl;

	for(size_t i=0;i<str.size();i++)
	{
		if(str[i]=='\n')
		{
			if(num==line)
				break;
			num++;
		}
		if(str[i]!='\n'&&str[i]!='\r'&&num==line)
			tl+=str[i];
	}

	return tl;
}

//--------------------------------------------------------------------
/**
*	zeilenanzahl andorfern (getrennt durch \n)
*/
s32 linecount(const std::string &str)
{
	s32 lines=0;
	for(size_t i=0;i<str.size();i++)
	{
		if(str[i]=='\n')
			lines++;
	}
	return lines+1;
}

//--------------------------------------------------------------------
/**
*	text datei auslesen
*/
std::string getFile(std::string filename)
{
		std::fstream FileBin;
		FileBin.open(filename.c_str(), std::ios::in|std::ios::binary);
		if(FileBin.is_open()==false)
        {
                return "";
        }
        FileBin.seekg(0, std::ios::end);
		unsigned long FileSize = (unsigned int)std::streamoff(FileBin.tellg());
        FileBin.seekg(0, std::ios::beg);
		std::string ret;
		ret.resize(FileSize);
        FileBin.read(const_cast<char*>(ret.c_str()), FileSize);
		FileBin.close();
        return ret;
}

std::string getStreamFile(const std::string& fn)
{
	std::fstream fin(fn.c_str(), std::ios::binary | std::ios::in);
	if (!fin.is_open())
		return std::string();

	std::string ret;
	while (!fin.eof())
	{
		char buf[512];
		fin.read(buf, sizeof(buf));
		ret.insert(ret.end(), buf, buf + fin.gcount());
	}
	return ret;
}

void strupper_utf8(std::string *pStr)
{
	std::wstring tmp;
	try
	{
		if (sizeof(wchar_t) == 2)
		{
			utf8::utf8to16(pStr->begin(), pStr->end(), back_inserter(tmp));
		}
		else
		{
			utf8::utf8to32(pStr->begin(), pStr->end(), back_inserter(tmp));
		}
	}
	catch (...) {}
	
#ifdef _WIN32
	CharUpperBuffW(&tmp[0], static_cast<DWORD>(tmp.size()));
#else
	for (size_t i = 0; i < tmp.size(); ++i)
	{
		tmp[i] = towupper(tmp[i]);
	}
#endif

	pStr->clear();
	try
	{
		if (sizeof(wchar_t) == 2)
		{
			utf8::utf16to8(tmp.begin(), tmp.end(), back_inserter(*pStr));
		}
		else
		{
			utf8::utf32to8(tmp.begin(), tmp.end(), back_inserter(*pStr));
		}
	}
	catch (...) {}
}

void strlower_utf8(std::string *pStr)
{
	std::wstring tmp;
	try
	{
		if (sizeof(wchar_t) == 2)
		{
			utf8::utf8to16(pStr->begin(), pStr->end(), back_inserter(tmp));
		}
		else
		{
			utf8::utf8to32(pStr->begin(), pStr->end(), back_inserter(tmp));
		}
	}
	catch (...) {}

#ifdef _WIN32
	CharLowerBuffW(&tmp[0], static_cast<DWORD>(tmp.size()));
#else
	for (size_t i = 0; i < tmp.size(); ++i)
	{
		tmp[i] = towlower(tmp[i]);
	}
#endif

	pStr->clear();
	try
	{
		if (sizeof(wchar_t) == 2)
		{
			utf8::utf16to8(tmp.begin(), tmp.end(), back_inserter(*pStr));
		}
		else
		{
			utf8::utf32to8(tmp.begin(), tmp.end(), back_inserter(*pStr));
		}
	}
	catch (...) {}
}

std::string strlower(const std::string &str)
{
	std::string ret;
	ret.resize(str.size());
	for (size_t i = 0; i < str.size(); ++i)
	{
		if (str[i] < 0)
		{
			ret = str;
			strlower_utf8(&ret);
			return ret;
		}
		else
		{
			ret[i] = tolower(str[i]);
		}
	}

	return ret;
}

void strupper(std::string *pStr)
{
	for(size_t i=0;i<pStr->size();++i)
	{
		char ch = (*pStr)[i];
		if (ch < 0)
		{
			strupper_utf8(pStr);
			return;
		}
		else
		{
			(*pStr)[i] = toupper((*pStr)[i]);
		}
	}
}

//--------------------------------------------------------------------
/**
*	dateiname aus pfadangabe extrahieren
*/
string ExtractFileName(string fulln, string separators)
{
	string filename;

	s32 off=0;
	for(s32 i=(s32)fulln.length()-1;i>-1;i--)
	{
		bool separator = separators.find(fulln[i])!=string::npos;
		if(separator)
		{
			if(i<(s32)fulln.length()-1)
				break;
		}
		if(fulln[i]!=0 && !separator)
			filename=fulln[i]+filename;
	}

	return filename;
}

//--------------------------------------------------------------------
/**
*	pfad aus pfadangabe (inkl. dateiname) extrahieren
*/
string ExtractFilePath(string fulln, string separators)
{
	bool in=false;
	string path;
	for(s32 i=(s32)fulln.length()-2;i>=0;--i)
	{
		if( separators.find(fulln[i])!=string::npos
			&& in==false)
		{
			in=true;
			continue;
		}
		
		if(in==true)
		{
			path=fulln[i]+path;
		}

	}

	return path;
}

//--------------------------------------------------------------------
/**
*	bool in wide string (true/false) konvertieren
*/
std::string convert(bool pBool)
{
	if(pBool)
		return "true";
	else 
		return "false";
}

//--------------------------------------------------------------------
/**
*	integer in string
*/
std::string convert(int i){
	ostringstream ss;
	ss << i;
	return ss.str();
}

//--------------------------------------------------------------------
/**
*	integer in string
*/
std::string convert(long long int i){
	ostringstream ss;
	ss << i;
	return ss.str();
}

#if defined(_WIN64) || defined(_LP64)
std::string convert(unsigned int i){
	ostringstream ss;
	ss << i;
	return ss.str();
}
#endif

//--------------------------------------------------------------------
/**
*	size_t in string
*/
std::string convert(size_t i){
	ostringstream ss;
	ss << i;
	return ss.str();
}

#if !defined(_WIN32) || !defined(_WIN64)
//--------------------------------------------------------------------
/**
*	integer in string
*/

std::string convert(unsigned long long int i){
	ostringstream ss;
	ss << i;
	return ss.str();
}

#endif

//--------------------------------------------------------------------
/**
*	f32 in string
*/
std::string convert(f32 f){
	ostringstream ss;
	ss << f;
	return ss.str();
}

std::string convert(double f){
	ostringstream ss;
	ss << f;
	return ss.str();
}

//--------------------------------------------------------------------
/**
*	datei-endung finden
*/
std::string findextension(const std::string& pString)
{
	std::string retv;
	std::string temp;

	for(s32 i=(s32)pString.size()-1; i>=0; i--)
		if( pString[i] != '.' )
			temp.push_back(pString[i]);
		else
			break;

	for(s32 i=(s32)temp.size()-1; i>=0; i--)
		retv.push_back(temp[i]);

	return retv;
}

//--------------------------------------------------------------------
/**
*/
std::string replaceonce(std::string tor, std::string tin, std::string data)
{
        s32 off=(s32)data.find(tor);
        if(off!=-1)
        {
                data.erase(off,tor.size() );
                data.insert(off,tin);
        }
        return data;
}

void Tokenize(const std::string& str, std::vector<std::string> &tokens, std::string seps)
{
	// one-space line for storing blank lines in the file
	std::string blankLine = "";

	// pos0 and pos1 store the scope of the current turn, i stores
	// the position of the symbol \".
	s32 pos0 = 0, pos1 = 0;
	while(true)
	{ 
		// find the next seperator
		pos1 = (s32)str.find_first_of(seps, pos0);
		// find the next \" 
	    
		// if the end is reached..
	    if(pos1 == std::string::npos)
	    {
			// ..push back the string to the end
			std::string nt=str.substr(pos0, str.size());
			if( nt!="" )
				tokens.push_back(nt);
			break;
	    }  
		// if two seperators are found in a row, the file has a blank
		// line, in this case the one-space string is pushed as a token
		else if( pos1==pos0 )
		{
			tokens.push_back(blankLine);
		}
	    else
            // if no match is found, we have a simple token with the range
			// stored in pos0/1
			tokens.push_back(str.substr(pos0, (pos1 - pos0)));

		// equalize pos
		pos0=pos1;
		// increase 
	    ++pos1;
		// added for ini-file!
		// increase by length of seps
		++pos0;
	}
}

//--------------------------------------------------------------------
/**
*/
bool str_isnumber(char ch)
{
	if( ch>=48 && ch <=57 )
		return true;
	else
		return false;
}

//--------------------------------------------------------------------
/**
*/
bool isletter(char ch)
{
	ch=toupper(ch);
	if( ch<=90 && ch>=65)
		return true;
	else
		return false;
}


bool next(const std::string &pData, const size_t & doff, const std::string &pStr)
{
        for(size_t i=0;i<pStr.size();++i)
        {
                if( i+doff>=pData.size() )
                        return false;
                if( pData[doff+i]!=pStr[i] )
                        return false;
        }
        return true;
}

std::string greplace(std::string tor, std::string tin, std::string data)
{
        for(size_t i=0;i<data.size();++i)
        {
                if( next(data, i, tor)==true )
                {
                        data.erase(i, tor.size());
                        data.insert(i,tin);
			i+=tin.size()-1;
                }
        }

        return data;
}

int getNextNumber(const std::string &pStr, int *read)
{
	std::string num;
	bool start=false;
	for(size_t i=0;i<pStr.size();++i)
	{
		if( str_isnumber(pStr[i] ) )
		{
			num+=pStr[i];
			start=true;
		}
		else if(start==true)
			return atoi(num.c_str() );

		if(read!=NULL)
			++*read;
	}

	return 0;
}

void transformHTML(std::string &str)
{
	for(size_t i=0;i<str.size();++i)
	{
		if( next(str, i, "$amp;" )==true )
		{
			str.erase(i,5);
			str.insert(i,"&");
		}
	}
}

std::string EscapeSQLString(const std::string &pStr)
{
	std::string ret;
	for(size_t i=0;i<pStr.size();++i)
	{
		if(pStr[i]=='\'')
		{
			ret+="''";
		}
		else
		{
			ret+=pStr[i];
		}
	}
	return ret;
}

std::string EscapeParamString(const std::string &pStr)
{
	std::string ret;
	ret.reserve(pStr.size());
	for(size_t i=0;i<pStr.size();++i)
	{
		switch(pStr[i])
		{
		case '%': ret+="%25"; break;
		case '&': ret+="%26"; break;
		case '$': ret+="%24"; break;
		case '/': ret+="%2F"; break;
		case ' ': ret+="%20"; break;
		case '#': ret+="%23"; break;
		case '+': ret+="%2B"; break;
		case '\n': ret += "%0A"; break;
		case '\r': ret += "%0D"; break;
		default: ret+=pStr[i]; break;
		}
	}
	return ret;
}

void EscapeCh(std::string &pStr, char ch)
{
	std::string ins;
	ins+=ch;
	for(size_t i=0;i<pStr.size();++i)
	{
		if(pStr[i]==ch)
		{
			pStr.insert(i,ins);
			++i;
		}
	}
}

std::string UnescapeSQLString(const std::string &pStr)
{
	std::string ret;
	for(size_t i=0;i<pStr.size();++i)
	{
		if( i+1<pStr.size() && pStr[i]=='\'' && pStr[i+1]=='\'' )
		{
			ret+="'";
			++i;
		}
		else
		{
			ret+=pStr[i];
		}
	}
	return ret;
}

void ParseParamStrHttp(const std::string &pStr, std::map<std::string,std::string> *pMap, bool escape_params)
{
	std::string key;
	std::string value;

	int pos=0;
	for(size_t i=0;i<pStr.size();++i)
	{
		char ch=pStr[i];
		if( ch=='=' && pos==0)
		{
			pos=1;
		}
		else if( (ch=='&'||ch=='$') && pos==1 )
		{
			pos=0;
			std::string wv=htmldecode(value, false);
			if(escape_params)
			{
				wv=EscapeSQLString(wv);
			}
			pMap->insert( std::pair<std::string, std::string>(key,wv) );
			key.clear(); value.clear();
		}
		else if( pos==0 )
		{
			key+=ch;
		}
		else if( pos==1 )
		{
			value+=ch;
		}
	}

	if( value.size()>0 || key.size()>0 )
	{
		std::string wv=htmldecode(value, false);
		if(escape_params)
		{
			wv=EscapeSQLString(wv);
		}
		pMap->insert( std::pair<std::string, std::string>(key,wv) );
	}
}

std::string FormatTime(int timeins)
{
	float t=(float)timeins;
	int h;
	int m;
	int s;

	h=int(t/3600.0f);
	m=int(t/60)-h*60;
	s=int(t-h*3600-m*60);

	std::string sh,sm,ss;
	sh=convert(h);
	sm=convert(m);
	ss=convert(s);
	if( sm.size()==1 && h>0 )
		sm="0"+sm;
	if( ss.size()==1 )
		ss="0"+ss;

	std::string ret=sm+":"+ss;

	if(h>0)
		ret=sh+":"+ret;

	return ret;
}

//-------------------HTML DECODE-----------------

namespace
{
	const char hexnum_array[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'}; 
}


bool IsHex(const std::string &str)
{
	bool hex=true;
	for(size_t i=0;i<str.size();++i)
	{
		bool in=false;
		for(size_t t=0;t<16;++t)
		{
			if(hexnum_array[t]==str[i])
			{
				in=true;
				break;
			}
		}
		if(in==false)
		{
			hex=false;
			break;
		}
	}
	return hex;
}

unsigned long hexToULong(const std::string &data)
{
	std::string str=strlower(data);
	unsigned int hex_length = (unsigned int)str.size();
	unsigned long return_value = 0;
	for(unsigned int i = 0; i < hex_length; i++)
	{
		for(unsigned char j = 0; j < 16; j++)
		{
			if( str[i] == hexnum_array[j])
			{			
				return_value = ((return_value * 16) + j);
			}
		}
	}
	return return_value;
}

std::string byteToHex(unsigned char ch)
{
	std::string r;
	r.resize(2);
	r[0]=hexnum_array[ch/16];
	r[1]=hexnum_array[ch%16];
	return r;
}

std::string bytesToHex(const unsigned char *b, size_t bsize)
{
	std::string r;
	r.reserve(bsize*2);
	for(size_t i=0;i<bsize;++i)
	{
		r+=byteToHex(b[i]);
	}
	return r;
}

std::string bytesToHex( const std::string& data )
{
	return bytesToHex(reinterpret_cast<const unsigned char*>(data.data()), data.size());
}

std::string hexToBytes(const std::string& data)
{
	std::string ret;
	ret.resize(data.size()/2);
	for(size_t i=0;i<data.size();i+=2)
	{
		ret[i/2]=static_cast<char>(hexToULong(data.substr(i, 2)));
	}
	return ret;
}

string htmldecode(string str, bool html, char xc)
{
	std::string ret;
	for(size_t i=0;i<str.size();i++)
	{
		if(str[i]==xc && i+2<str.size())
		{
			std::string data; data.push_back(str[i+1]); data.push_back(str[i+2]);
			unsigned char ch=(unsigned char)hexToULong(data);
			if( html==true && ch!=0  )
			{
				if( ch!='-' && ch!=',' && ch!='#' )
					ret+="&#"+convert((s32)ch)+";";
				else
				{
					ret+=ch;
				}
			}
			else if( ch!=0 )
			{
				ret+=ch;
			}
			i+=2;
		}
		else if(str[i]=='+' && !html)
		{
			ret+=' ';
		}
		else
		{
			ret+=str[i];
		}
	}
	return ret;
}

bool checkhtml(const std::string &str)
{
	for(size_t i=0;i<str.size();++i)
	{
		char ch=str[i];
		if( ch=='<' || ch=='>' || ch=='&')
			return false;
	}

	return true;
}

std::string nl2br(std::string str)
{
	for(size_t i=0;i<str.size();++i)
	{
		if( str[i]=='\n' )
		{
			str.erase(i,1);
			str.insert(i,"<br>");
		}
		else if( next(str,i,"&#10;")==true )
		{
			str.erase(i,5);
			str.insert(i,"<br>");
		}
	}

	return str;
}

bool FileExists(std::string pFile)
{
	fstream in(pFile.c_str(), ios::in);
	if( in.is_open()==false )
		return false;

	in.close();
	return true;
}

bool checkStringHTML(const std::string &str)
{
	for(size_t i=0;i<str.size();++i)
	{
		char ch=str[i];
		bool ok=false;
		if( ch>=48 && ch <=57)
			ok=true;
		else if( ch >=65 && ch <=90 )
			ok=true;
		else if( ch >=97 && ch <=122 )
			ok=true;
		else if(ch==95 || ch==46 || ch==45 )
			ok=true;

		if( ok==false )
			return false;
	}
	return true;
}

std::string ReplaceChar(std::string str, char tr, char ch)
{
	for(size_t i=0;i<str.size();++i)
	{
		if( str[i]==tr )
			str[i]=ch;
	}
	return str;
}

std::string striptags(std::string html)
{
	std::string ret;
	ret.reserve(html.size() );
	bool in=false;
	for(size_t i=0;i<html.size();++i)
	{
		if( html[i]=='<' )
			in=true;
		
		if( html[i]=='>' )
			in=false;
		else if(in==false )
		{
			ret+=html[i];
		}
	}
	return ret;
}

static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";


static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
  std::string ret;
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  while (in_len--) {
    char_array_3[i++] = *(bytes_to_encode++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for(i = 0; (i <4) ; i++)
        ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i)
  {
    for(j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; (j < i + 1); j++)
      ret += base64_chars[char_array_4[j]];

    while((i++ < 3))
      ret += '=';

  }

  return ret;

}

std::string base64_encode_dash(const std::string& data)
{
	std::string ret = base64_encode(reinterpret_cast<const unsigned char*>(data.c_str()),
		static_cast<unsigned int>(data.size()));

	for(size_t i=0;i<ret.size();++i)
	{
		if(ret[i]=='=')
			ret[i]='-';
	}

	return ret;
}

std::string base64_decode(std::string const& encoded_string) {
  int in_len = (int)encoded_string.size();
  int i = 0;
  int j = 0;
  int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::string ret;

  while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_]; in_++;
    if (i ==4) {
      for (i = 0; i <4; i++)
        char_array_4[i] = (unsigned char)base64_chars.find(char_array_4[i]);

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); i++)
        ret += char_array_3[i];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j <4; j++)
      char_array_4[j] = 0;

    for (j = 0; j <4; j++)
      char_array_4[j] = (unsigned char)base64_chars.find(char_array_4[j]);

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
  }

  return ret;
}

std::string base64_decode_dash(std::string s)
{
	for(size_t i=0;i<s.size();++i)
	{
		if(s[i]=='-')
		{
			s[i]='=';
		}
		else if(s[i]==' ')
		{
			s[i]='+';
		}
	}

	return base64_decode(s);
}

bool CheckForIllegalChars(const std::string &str)
{
	for(size_t i=0;i<str.size();++i)
	{
		if( str[i]==0 )
			return false;
	}

	return true;
}

int watoi(std::string str)
{
	return atoi(str.c_str());
}

_i64 watoi64(std::string str)
{
#ifdef _WIN32
	return _atoi64(str.c_str());
#else
	return atoll(str.c_str());
#endif
}

std::string trim(const std::string &str)
{
	size_t startpos = str.find_first_not_of(" \r\n\t");
	size_t endpos = str.find_last_not_of(" \r\n\t");
	if(( string::npos == startpos ) || ( string::npos == endpos))  
	{
		return "";
	}
	else
	{
		return str.substr( startpos, endpos-startpos+1 );
	}
}

void replaceNonAlphaNumeric(std::string &str, char rch)
{
	for(size_t i=0;i<str.size();++i)
	{
		if(isletter(str[i])==false && str_isnumber(str[i])==false)
		{
			str[i]=rch;
		}
	}
}

std::string conv_filename(std::string fn)
{
        for(size_t i=0;i<fn.size();++i)
        {
                char ch=fn[i];
                if( ch==':' || ch=='|' || ch=='&' || ch=='\\' || ch=='/' || ch=='?' || ch=='*' || ch=='"' || ch=='<' || ch=='>' )
                        fn[i]='_';
        }

        return fn;
}

std::string UnescapeHTML(const std::string &html)
{
	std::string ret=greplace("&amp;", "&", html);
	ret=greplace("&lt;", "<", ret);
	ret=greplace("&gt;", ">", ret);
	ret=greplace("&quot;", "\"", ret);
	ret=greplace("&#x27;", "'", ret);
	ret = greplace("&#x2F", "/", ret);
	return ret;
}

std::string EscapeHTML(const std::string & html)
{
	std::string ret;
	ret.reserve(html.size());
	for (size_t i = 0; i < html.size(); ++i)
	{
		if (html[i] == '<') ret += "&lt;";
		else if (html[i] == '>') ret += "&gt;";
		else if (html[i] == '&') ret += "&amp;";
		else if (html[i] == '\"') ret += "&quot;";
		else if (html[i] == '\'') ret += "&#x27;";
		else if (html[i] == '/') ret += "&#x2F";
		else ret += html[i];
	}
	return ret;
}

std::string PrettyPrintBytes(_i64 bytes)
{
	if(bytes<1024)
		return convert(bytes)+" bytes";

	if(bytes<1024*1024)
		return convert(bytes/1024.f)+" KB";

	if(bytes<1024*1024*1024)
		return convert(bytes/(1024.f*1024.f))+" MB";

	if((float)bytes<1024.f*1024*1024*1024)
		return convert(bytes/(1024.f*1024.f*1024.f))+" GB";

	return convert(bytes/(1024.f*1024.f*1024.f*1024.f))+" TB";
}

std::string PrettyPrintSpeed(size_t bps)
{
	size_t bit_ps=bps*8;

	if( bit_ps<1000)
		return convert(bit_ps)+" bps";

	if( bit_ps<1000*1000)
		return convert(bit_ps/1000.f)+" Kbps";

	if( bit_ps<1000*1000*1000)
		return convert(bit_ps/(1000.f*1000.f))+" Mbps";

	return convert(bit_ps/(1000.f*1000.f*1000.f))+" Gbps";
}

std::string PrettyPrintTime(int64 ms)
{
	std::string ret;

	unsigned int c_s=1000;
	unsigned int c_m=c_s*60;
	unsigned int c_h=c_m*60;
	unsigned int c_d=c_h*24;

	int64 orig = ms;

	if( ms>c_d)
	{
		int64 t=ms/c_d;
		if(!ret.empty()) ret+=" ";
		ret+=convert(t)+" days";
		ms-=t*c_d;
	}

	if( ms>c_h)
	{
		int64 t=ms/c_h;
		if(!ret.empty()) ret+=" ";
		ret+=convert(t)+"h";
		ms-=t*c_h;
	}

	if( ms>c_m)
	{
		int64 t=ms/c_m;
		if(!ret.empty()) ret+=" ";
		ret+=convert(t)+"m";
		ms-=t*c_m;
	}

	if( ms>c_s)
	{
		int64 t=ms/c_s;
		if(!ret.empty()) ret+=" ";
		ret+=convert(t)+"s";
		ms-=t*c_s;
	}

	if( orig < c_s)
	{
		if(!ret.empty()) ret+=" ";
		ret+=convert(ms)+"ms";
	}

	return ret;
}
