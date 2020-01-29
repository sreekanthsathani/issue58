#pragma once
#include "../Interface/File.h"
#include "../Interface/Thread.h"
#include "../Interface/Mutex.h"
#include "../Interface/Pipe.h"
#include "../idrivebmrcommon/sha2/sha2.h"
#include <memory>

class IPipeFile : public IFile
{
public:
	virtual int64 getLastRead() = 0;
	virtual bool getHasError() = 0;
	virtual std::string getStdErr() = 0;
	virtual bool getExitCode(int& exit_code) = 0;
	virtual void forceExitWait() = 0;
	virtual void addUser() = 0;
	virtual void removeUser() = 0;
	virtual bool hasUser() = 0;
	virtual int64 getPos() = 0;
};

class PipeFileBase : public IPipeFile, public IThread
{
public:
	PipeFileBase(const std::string& pCmd);
	~PipeFileBase();

	virtual void operator()();

	virtual std::string Read(_u32 tr, bool *has_error=NULL);

	virtual std::string Read(int64 spos, _u32 tr, bool *has_error = NULL);

	virtual _u32 Read(char* buffer, _u32 bsize, bool *has_error=NULL);

	virtual _u32 Read(int64 spos, char* buffer, _u32 bsize, bool *has_error = NULL);

	virtual _u32 Write(const std::string &tw, bool *has_error=NULL);

	virtual _u32 Write(int64 spos, const std::string &tw, bool *has_error = NULL);

	virtual _u32 Write(const char* buffer, _u32 bsiz, bool *has_error=NULL);

	virtual _u32 Write(int64 spos, const char* buffer, _u32 bsiz, bool *has_error = NULL);

	virtual bool Seek(_i64 spos);

	virtual _i64 Size(void);

	virtual _i64 RealSize();

	virtual std::string getFilename(void);

	int64 getLastRead();

	bool getHasError();

	std::string getStdErr();

	virtual bool PunchHole( _i64 spos, _i64 size );

	virtual bool Sync();

	virtual void addUser();

	virtual void removeUser();

	virtual bool hasUser();

	virtual int64 getPos();

protected:

	bool SeekInt(_i64 spos);

	void init();

	virtual bool readStdoutIntoBuffer(char* buf, size_t buf_avail, size_t& read_bytes) = 0;
	virtual void finishStdout() = 0;
	virtual bool readStderrIntoBuffer(char* buf, size_t buf_avail, size_t& read_bytes) = 0;

	bool has_error;

	void waitForExit();

	virtual void cleanupOnForceShutdown();

private:
	bool fillBuffer();
	bool readStderr();

	size_t getReadAvail();
	void readBuf(char* buf, size_t toread);


	std::string cmd;

	int64 curr_pos;
	size_t buf_w_pos;
	size_t buf_w_reserved_pos;
	size_t buf_r_pos;
	bool buf_circle;
	std::vector<char> buffer;
	std::string stderr_ret;
	std::auto_ptr<IMutex> buffer_mutex;
	size_t threadidx;
	bool has_eof;
	int64 stream_size;

	sha_def_ctx sha_ctx;

	int64 last_read;

	THREADPOOL_TICKET stdout_thread;
	THREADPOOL_TICKET stderr_thread;

	size_t n_users;
};

class ScopedPipeFileUser
{
public:
	ScopedPipeFileUser()
		: pipe_file(NULL)
	{}

	ScopedPipeFileUser(IPipeFile& p_pipe_file)
		: pipe_file(&p_pipe_file)
	{
		pipe_file->addUser();
	}

	~ScopedPipeFileUser()
	{
		if (pipe_file != NULL)
		{
			pipe_file->removeUser();
		}
	}

	IPipeFile* get()
	{
		return pipe_file;
	}

	void reset(IPipeFile* new_pipe_file)
	{
		if (pipe_file != NULL)
		{
			pipe_file->removeUser();
		}

		pipe_file = new_pipe_file;

		if (pipe_file != NULL)
		{
			pipe_file->addUser();
		}
	}

private:
	IPipeFile* pipe_file;
};