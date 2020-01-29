#pragma once

#include <memory>
#include "../Interface/File.h"

class IExtentIterator
{
public:
	virtual IFsFile::SSparseExtent nextExtent() = 0;

	virtual void reset() = 0;
};


class ExtentIterator : public IExtentIterator
{
public:
	ExtentIterator(IFile* sparse_extents_f, bool take_file_ownership=true, int64 blocksize = 1);
	~ExtentIterator();

	virtual IFsFile::SSparseExtent nextExtent();

	virtual void reset();

private:
	std::auto_ptr<IFile> sparse_extents_f;
	int64 num_sparse_extents;
	int64 next_sparse_extent_num;
	bool take_file_ownership;
	int64 blocksize;
};

class FsExtentIterator : public IExtentIterator
{
public:
	FsExtentIterator(IFsFile* backing_file, int64 blocksize=1);

	virtual IFsFile::SSparseExtent nextExtent();

	virtual void reset();

private:
	IFsFile* backing_file;
	int64 blocksize;
};