#include <string>

class IDatabase;

int cleanup_cmd(void);
int64 cleanup_amount(std::string cleanup_pc, IDatabase *db, bool isZFS = false);
int remove_unknown(void);
int cleanup_database(void);
int defrag_database(void);
int64 get_zfs_total_space();
std::string exec_cmd(const char* cmd);
int64 get_zfs_free_space();
