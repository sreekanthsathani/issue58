#ifndef CQUERY_H_
#define CQUERY_H_

#include "Interface/Query.h"
#include "Interface/Mutex.h"
#include "Interface/SharedMutex.h"
#include <memory>

struct sqlite3_stmt;
struct sqlite3;

class CDatabase;
class DatabaseCursor;

#define LOG_WRITE_QUERIES
#define LOG_READ_QUERIES

#if defined(LOG_WRITE_QUERIES) || defined(LOG_READ_QUERIES)
#define LOG_QUERIES
#endif

class ScopedAddActiveQuery;

class CQuery : public IQuery
{
public:
	CQuery(const std::string &pStmt_str, sqlite3_stmt *prepared_statement, CDatabase *pDB);
	~CQuery();

	static void init_mutex(void);

	virtual void Bind(const std::string &str);
	virtual void Bind(int p);
	virtual void Bind(unsigned int p);
	virtual void Bind(double p);
	virtual void Bind(int64 p);
#if defined(_WIN64) || defined(_LP64)
	virtual void Bind(size_t p);
#endif
	virtual void Bind(const char* buffer, _u32 bsize);

	virtual void Reset(void);

	virtual bool Write(int timeoutms=-1);
	db_results Read(int *timeoutms=NULL);

	virtual IDatabaseCursor* Cursor(int *timeoutms=NULL);

	void setupStepping(int *timeoutms, bool with_read_lock);
	void shutdownStepping(int err, int *timeoutms, bool& transaction_lock);

	int step(db_single_result& res, int *timeoutms, int& tries, bool& transaction_lock, bool& reset);

	bool resultOkay(int rc);

	std::string getStatement(void);

	std::string getErrMsg(void);

private:
	bool Execute(int timeoutms);

	void addActiveQuery(const std::string& query_str);
	void removeActiveQuery(const std::string& query_str);
	void showActiveQueries(int loglevel);

	sqlite3_stmt *ps;
	std::string stmt_str;
	CDatabase *db;
	int curr_idx;
	DatabaseCursor *cursor;

#ifdef LOG_QUERIES
	static IMutex* active_mutex;
	static std::vector<std::string> active_queries;
#endif

	std::auto_ptr<IScopedReadLock> single_use_lock;

	friend class ScopedAddActiveQuery;
};

class CQuery;

class ScopedAddActiveQuery
{
public:
	ScopedAddActiveQuery(CQuery* query)
		: query(query)
	{
#ifdef LOG_QUERIES
		query->addActiveQuery(query->stmt_str);
#endif
	}

	~ScopedAddActiveQuery()
	{
#ifdef LOG_QUERIES
		query->removeActiveQuery(query->stmt_str);
#endif
	}

private:
	CQuery* query;
};

#endif //CQUERY_H_