#pragma once
#ifndef __APPLE__

#include "IVHDFile.h"

class CowFile : public IVHDFile
{
public:
	CowFile(const std::string &fn, bool pRead_only, uint64 pDstsize);
	CowFile(const std::string &fn, const std::string &parent_fn, bool pRead_only, uint64 pDstsize);
	~CowFile();


	virtual bool Seek(_i64 offset);
	virtual bool Read(char* buffer, size_t bsize, size_t &read_bytes);
	virtual _u32 Write(const char *buffer, _u32 bsize, bool *has_error);
	virtual bool isOpen(void);
	virtual uint64 getSize(void);
	virtual uint64 usedSize(void);
	virtual std::string getFilename(void);
	virtual bool has_sector(_i64 sector_size=-1);
	virtual bool this_has_sector(_i64 sector_size=-1);
	virtual unsigned int getBlocksize();
	virtual bool finish();
	virtual bool trimUnused(_i64 fs_offset, _i64 trim_blocksize, ITrimCallback* trim_callback);
	virtual bool syncBitmap(_i64 fs_offset);
	virtual bool makeFull(_i64 fs_offset, IVHDWriteCallback* write_callback) { return true; }
	virtual bool setUnused(_i64 unused_start, _i64 unused_end);
	virtual bool setBackingFileSize(_i64 fsize);

private:
	void setupBitmap();
	bool isBitmapSet(uint64 offset);
	void setBitmapBit(uint64 offset, bool v);
	void setBitmapRange(uint64 offset_start, uint64 offset_end, bool v);
	bool hasBitmapRangeNarrow(uint64& offset_start, uint64& offset_end, uint64 trim_blocksize);
	bool saveBitmap();
	bool loadBitmap(const std::string& bitmap_fn);
	void resizeBitmap();

	int fd;
	std::string filename;
	bool read_only;
	bool is_open;
	bool bitmap_dirty;

	uint64 filesize;
	_i64 curr_offset;

	std::vector<unsigned char> bitmap;

	bool finished;
	bool trim_warned;
	std::vector<char> zero_buf;
};

#endif //__APPLE__
