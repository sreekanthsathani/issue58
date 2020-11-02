#ifndef STRINGTOOLS_H
#define STRINGTOOLS_H

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "Types.h"
std::string getafter(const std::string &str,const std::string &data);
std::string getafterinc(const std::string &str,const std::string &data);
std::string getafter(const std::string &str,const std::string &data);
std::string getafterinc(const std::string &str,const std::string &data);
std::string getbetween(std::string s1,std::string s2,std::string data);
std::string strdelete(std::string str,std::string data);
void writestring(std::string str,std::string file);
void writestring(char *str, unsigned int len,std::string file);
std::string getuntil(std::string str,std::string data);
std::string getuntil(std::string str,std::string data);
std::string getuntilinc(std::string str,std::string data);
std::string getline(int line,const std::string &str);
int linecount(const std::string &str);
std::string getFile(std::string filename);
std::string getStreamFile(const std::string& fn);
std::string ExtractFileName(std::string fulln, std::string separators="/\\");
std::string ExtractFilePath(std::string fulln, std::string separators="/\\");
std::string convert(bool pBool);
std::string convert(int i);
std::string convert(float f);
std::string convert(double f);
std::string convert(long long int i);
std::string convert(size_t i);
#if !defined(_WIN64)
std::string convert(unsigned long long int i);
#endif
std::string convert(unsigned int i);
std::string findextension(const std::string& pString);
std::string replaceonce(std::string tor, std::string tin, std::string data);
void Tokenize(const std::string& str, std::vector<std::string> &tokens, std::string seps);
bool str_isnumber(char ch);
bool isletter(char ch);
void strupper(std::string *pStr);
std::string greplace(std::string tor, std::string tin, std::string data);
int getNextNumber(const std::string &pStr, int *read=NULL);
std::string strlower(const std::string &str);
bool next(const std::string &pData, const size_t & doff, const std::string &pStr);
void transformHTML(std::string &str);
std::string EscapeSQLString(const std::string &pStr);
void EscapeCh(std::string &pStr, char ch='\\');
std::string UnescapeSQLString(const std::string &pStr);
void ParseParamStrHttp(const std::string &pStr, std::map<std::string,std::string> *pMap, bool escape_params=false);
std::string FormatTime(int timeins);
bool IsHex(const std::string &str);
unsigned long hexToULong(const std::string &data);
std::string byteToHex(unsigned char ch);
std::string bytesToHex(const unsigned char *b, size_t bsize);
std::string bytesToHex(const std::string& data);
std::string hexToBytes(const std::string& data);
std::string htmldecode(std::string str, bool html=true, char xc='%');
bool checkhtml(const std::string &str);
std::string nl2br(std::string str);
bool FileExists(std::string pFile);
bool checkStringHTML(const std::string &str);
std::string ReplaceChar(std::string str, char tr, char ch);
std::string striptags(std::string html);
std::string base64_encode(unsigned char const* , unsigned int len);
std::string base64_encode_dash(const std::string& data);
std::string base64_decode(std::string const& s);
std::string base64_decode_dash(std::string s);
bool CheckForIllegalChars(const std::string &str);
int watoi(std::string str);
_i64 watoi64(std::string str);
std::string trim(const std::string &str);
void replaceNonAlphaNumeric(std::string &str, char rch);
std::string conv_filename(std::string fn);
std::string UnescapeHTML(const std::string &html);
std::string EscapeHTML(const std::string &html);
std::string PrettyPrintBytes(_i64 bytes);
std::string PrettyPrintSpeed(size_t bps);
std::string PrettyPrintTime(int64 ms);
std::string EscapeParamString(const std::string &pStr);

namespace
{

bool is_big_endian(void)
{
    union {
        unsigned int i;
        char c[4];
    } bint = {0x01020304};

    return bint.c[0] == 1;
}

unsigned int endian_swap(unsigned int x)
{
    return (x>>24) |
        ((x<<8) & 0x00FF0000) |
        ((x>>8) & 0x0000FF00) |
        (x<<24);
}

unsigned short endian_swap(unsigned short x)
{
    return x = (x>>8) |
        (x<<8);
}

std::string endian_swap_utf16(std::string str)
{
	for(size_t i=0;i<str.size();i+=2)
	{
		unsigned short *t=(unsigned short*)&str[i];
		*t=endian_swap(*t);
	}
	return str;
}

uint64 endian_swap(uint64 x)
{
#ifdef _WIN32
    return (x>>56) |
        ((x<<40) & 0x00FF000000000000) |
        ((x<<24) & 0x0000FF0000000000) |
        ((x<<8)  & 0x000000FF00000000) |
        ((x>>8)  & 0x00000000FF000000) |
        ((x>>24) & 0x0000000000FF0000) |
        ((x>>40) & 0x000000000000FF00) |
        (x<<56);
#else
    return (x>>56) |
        ((x<<40) & 0x00FF000000000000LLU) |
        ((x<<24) & 0x0000FF0000000000LLU) |
        ((x<<8)  & 0x000000FF00000000LLU) |
        ((x>>8)  & 0x00000000FF000000LLU) |
        ((x>>24) & 0x0000000000FF0000LLU) |
        ((x>>40) & 0x000000000000FF00LLU) |
        (x<<56);
#endif
}

unsigned int little_endian(unsigned int x)
{
	if(is_big_endian())
	{
		return endian_swap(x);
	}
	else
	{
		return x;
	}
}

unsigned short little_endian(unsigned short x)
{
	if(is_big_endian())
	{
		return endian_swap(x);
	}
	else
	{
		return x;
	}
}

int little_endian(int x)
{
	if(is_big_endian())
	{
		return static_cast<int>(endian_swap(static_cast<unsigned int>(x)));
	}
	else
	{
		return x;
	}
}

uint64 little_endian(uint64 x)
{
	if(is_big_endian())
	{
		return endian_swap(x);
	}
	else
	{
		return x;
	}
}

int64 little_endian(int64 x)
{
	if(is_big_endian())
	{
		return static_cast<int64>(endian_swap(static_cast<uint64>(x)));
	}
	else
	{
		return x;
	}
}

float little_endian(float x)
{
	if(is_big_endian())
	{
		unsigned int* ptr=reinterpret_cast<unsigned int*>(&x);
		unsigned int ret = endian_swap(*ptr);
		return *reinterpret_cast<float*>(&ret);
	}
	else
	{
		return x;
	}
}

double little_endian(double x)
{
	if (is_big_endian())
	{
		uint64* ptr = reinterpret_cast<uint64*>(&x);
		uint64 ret = endian_swap(*ptr);
		return *reinterpret_cast<double*>(&ret);
	}
	else
	{
		return x;
	}
}

unsigned int big_endian(unsigned int x)
{
	if(!is_big_endian())
	{
		return endian_swap(x);
	}
	else
	{
		return x;
	}
}

unsigned short big_endian(unsigned short x)
{
	if(!is_big_endian())
	{
		return endian_swap(x);
	}
	else
	{
		return x;
	}
}

int big_endian(int x)
{
	if(!is_big_endian())
	{
		return static_cast<int>(endian_swap(static_cast<unsigned int>(x)));
	}
	else
	{
		return x;
	}
}

uint64 big_endian(uint64 x)
{
	if(!is_big_endian())
	{
		return endian_swap(x);
	}
	else
	{
		return x;
	}
}

int64 big_endian(int64 x)
{
	if(!is_big_endian())
	{
		return static_cast<int64>(endian_swap(static_cast<uint64>(x)));
	}
	else
	{
		return x;
	}
}

float big_endian(float x)
{
	if(!is_big_endian())
	{
		unsigned int* ptr=reinterpret_cast<unsigned int*>(&x);
		unsigned int ret = endian_swap(*ptr);
		return *reinterpret_cast<float*>(&ret);
	}
	else
	{
		return x;
	}
}

std::string big_endian_utf16(std::string str)
{
	if(!is_big_endian())
	{
		return endian_swap_utf16(str);
	}
	else
	{
		return str;
	}
}

}

#endif
