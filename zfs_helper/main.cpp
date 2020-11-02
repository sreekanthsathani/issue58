#include <string>
#include <string>
#include <iostream>
#include <vector>
#include "stringtools.h"
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>

extern char **environ;
#endif

bool create_subvolume_with_encryption(std::string subvolume_folder, std::string encryption_key);

const int mode_zfs=1;

std::string db_manager_path = "/usr/local/DBManager";

std::string os_file_sep(void)
{
	return "/";
}


bool os_create_dir(const std::string &path)
{
	return mkdir(path.c_str(), S_IRWXU | S_IRWXG)==0;
}


bool os_remove_dir(const std::string &path)
{
	return rmdir(path.c_str())==0;
}


std::string getBackupfolderPath(int mode)
{
	std::string fn_name;
	if(mode==mode_zfs)
	{
		fn_name = "dataset";
	}
	else
	{
		return std::string();
	}

	std::string fn;
#ifdef _WIN32
	fn=trim(getFile(fn_name));
#else
	fn=trim(getFile("/etc/idrivebmr/"+fn_name));
#endif
	if(fn.find("\n")!=std::string::npos)
		fn=getuntil("\n", fn);
	if(fn.find("\r")!=std::string::npos)
		fn=getuntil("\r", fn);

	return fn+"/vmware";
}

std::string handleFilename(std::string fn)
{
	fn=conv_filename(fn);
	if(fn=="..")
	{
		return "";
	}
	return fn;
}

#ifndef _WIN32
int exec_wait(const std::string& path, bool keep_stdout, ...)
{
   // std::cout << "Inside exec_wait cmd is " << path << std::endl;

	va_list vl;
	va_start(vl, keep_stdout);

	std::vector<char*> args;
	args.push_back(const_cast<char*>(path.c_str()));

	while(true)
	{
		const char* p = va_arg(vl, const char*);
		if(p==NULL) break;
		args.push_back(const_cast<char*>(p));
		//std::cout << "arg is " << p << std::endl;
	}
	va_end(vl);

	args.push_back(NULL);

	pid_t child_pid = fork();

	if(child_pid==0)
	{
		environ = new char*[1];
		*environ=NULL;

		if(!keep_stdout)
		{
			int nullfd = open("/dev/null", O_WRONLY);

			if(nullfd!=-1)
			{
				if(dup2(nullfd, 1)==-1)
				{
					return -1;
				}

				if(dup2(nullfd, 2)==-1)
				{
					return -1;
				}
			}
			else
			{
				return -1;
			}
		}
        int rc = execvp(path.c_str(), args.data());
        exit(rc);
	}
	else
	{
		int status;
		waitpid(child_pid, &status, 0);
		if(WIFEXITED(status))
		{
			return WEXITSTATUS(status);
		}
		else
		{
			return -1;
		}
	}
}

int exec_wait(const std::string& path, std::string& stdout, ...)
{
	va_list vl;
	va_start(vl, stdout);

	std::vector<char*> args;
	args.push_back(const_cast<char*>(path.c_str()));

	while(true)
	{
		const char* p = va_arg(vl, const char*);
		if(p==NULL) break;
		args.push_back(const_cast<char*>(p));
	}
	va_end(vl);

	args.push_back(NULL);

	int pipefd[2];
	if (pipe(pipefd) == -1)
	{
		return -1;
	}

	pid_t child_pid = fork();

	if(child_pid==0)
	{
		environ = new char*[1];
		*environ=NULL;

		close(pipefd[0]);

		if(dup2(pipefd[1], 1)==-1)
		{
			return -1;
		}

		close(pipefd[1]);

		int rc = execvp(path.c_str(), args.data());
		exit(rc);
	}
	else
	{
		close(pipefd[1]);

		char buf[512];
		int r;
		while( (r=read(pipefd[0], buf, 512))>0)
		{
			stdout.insert(stdout.end(), buf, buf+r);
		}

		close(pipefd[0]);

		int status;
		waitpid(child_pid, &status, 0);
		if(WIFEXITED(status))
		{
			return WEXITSTATUS(status);
		}
		else
		{
			return -1;
		}
	}
}

bool chown_dir(const std::string& dir)
{
	passwd* user_info = getpwnam("idrivebmr");
	if(user_info)
	{
		int rc = chown(dir.c_str(), user_info->pw_uid, user_info->pw_gid);
		return rc!=-1;
	}
	return false;
}

std::string find_zfs_cmd()
{
	static std::string zfs_cmd;

	if(!zfs_cmd.empty())
	{
		//return zfs_cmd;
	}
	else if(exec_wait("zfs", false, "--version", NULL)==2)
	{
		zfs_cmd="zfs";
		//return zfs_cmd;
	}
	else if(exec_wait("/sbin/zfs", false, "--version", NULL)==2)
	{
		zfs_cmd="/sbin/zfs";
		//return zfs_cmd;
	}
	else if(exec_wait("/bin/zfs", false, "--version", NULL)==2)
	{
		zfs_cmd="/bin/zfs";
		//return zfs_cmd;
	}
	else if(exec_wait("/usr/sbin/zfs", false, "--version", NULL)==2)
	{
		zfs_cmd="/usr/sbin/zfs";
		//return zfs_cmd;
	}
	else if(exec_wait("/usr/bin/zfs", false, "--version", NULL)==2)
	{
		zfs_cmd="/usr/bin/zfs";
		//return zfs_cmd;
	}
	else
	{
		zfs_cmd="zfs";
		//return zfs_cmd;
	}

	//std::cout << "zfs_cmd is " << zfs_cmd << std::endl;

	return zfs_cmd;
}

std::string find_zpool_cmd()
{
	static std::string zpool_cmd;

	if(!zpool_cmd.empty())
	{
		return zpool_cmd;
	}

	if(exec_wait("zpool", false, "--version", NULL)==2)
	{
		zpool_cmd="zpool";
		return zpool_cmd;
	}
	else if(exec_wait("/sbin/zpool", false, "--version", NULL)==2)
	{
		zpool_cmd="/sbin/zpool";
		return zpool_cmd;
	}
	else if(exec_wait("/bin/zpool", false, "--version", NULL)==2)
	{
		zpool_cmd="/bin/zpool";
		return zpool_cmd;
	}
	else if(exec_wait("/usr/sbin/zpool", false, "--version", NULL)==2)
	{
		zpool_cmd="/usr/sbin/zpool";
		return zpool_cmd;
	}
	else if(exec_wait("/usr/bin/zpool", false, "--version", NULL)==2)
	{
		zpool_cmd="/usr/bin/zpool";
		return zpool_cmd;
	}
	else
	{
		zpool_cmd="zpool";
		return zpool_cmd;
	}
}

#endif

bool create_subvolume(int mode, std::string subvolume_folder)
{
    if(mode==mode_zfs)
	{
		int rc=exec_wait(find_zfs_cmd(), true, "create", "-p", subvolume_folder.c_str(), NULL);
		chown_dir(subvolume_folder);
		return rc==0;
	}
	return false;
}


bool exec_cmd_with_input(std::string strCmd, std::string args)
{
    FILE *f;

    std::string createCmd = strCmd;
    f = popen (createCmd.c_str(), "w");
    if (!f)
    {
        perror ("popen");
        exit(1);
    }

    fprintf (f, "%s\n",args.c_str());

    bool rc = pclose (f);
    return rc;
}

bool get_mountpoint(int mode, std::string subvolume_folder)
{
    if(mode==mode_zfs)
	{
		int rc=exec_wait(find_zfs_cmd(), true, "get", "-H", "-o", "value", "mountpoint", subvolume_folder.c_str(), NULL);
		return rc==0;
	}
	return false;
}

bool make_readonly(int mode, std::string subvolume_folder)
{
    if(mode==mode_zfs)
	{
		int rc=exec_wait(find_zfs_cmd(), true, "snapshot", (subvolume_folder+"@ro").c_str(), NULL);
		return rc==0;
	}
	return false;
}

bool make_readonly_custom(int mode, std::string subvolume_folder, std::string readonlyname)
{
    if(mode==mode_zfs)
	{
		int rc=exec_wait(find_zfs_cmd(), true, "snapshot", (subvolume_folder+"@"+readonlyname).c_str(), NULL);
		return rc==0;
	}
	return false;
}

bool is_subvolume(int mode, std::string subvolume_folder)
{
    if(mode==mode_zfs)
	{
		int rc=exec_wait(find_zfs_cmd(), false, "list", subvolume_folder.c_str(), NULL);
		return rc==0;
	}
	return false;
}

bool create_snapshot(int mode, std::string snapshot_src, std::string snapshot_dst)
{
    std::cout << snapshot_src.c_str() << "  " << snapshot_dst.c_str() << std::endl;
    if(mode==mode_zfs)
	{
        std::cout << "Checking if subvolume exists " << snapshot_src+"@ro" << std::endl;
        if(!is_subvolume(mode,snapshot_src+"@ro"))
		{
           std::cout << "Not existing.. creating readonly snapshot " << snapshot_src+"@ro" << std::endl;
           int rc = make_readonly(mode,snapshot_src);
           std::cout << " made readonly snapshot returned " << rc << std::endl;
        }

        std::cout << " Creating clone " << snapshot_src <<  "  " << snapshot_dst << std::endl;
		int rc=exec_wait(find_zfs_cmd(), true, "clone", (snapshot_src+"@ro").c_str(), snapshot_dst.c_str(), NULL);
		chown_dir(snapshot_dst);
		return rc==0;
	}
	return false;
}

bool promote_dependencies(const std::string& snapshot, std::vector<std::string>& dependencies)
{
	std::cout << "Searching for origin " << snapshot << std::endl;

	std::string snap_data;
	int rc = exec_wait(find_zfs_cmd(), snap_data, "list", "-H", "-o", "name", NULL);
	if(rc!=0)
		return false;

	std::vector<std::string> snaps;
	Tokenize(snap_data, snaps, "\n");

	std::string snap_folder = ExtractFilePath(snapshot);
	for(size_t i=0;i<snaps.size();++i)
	{
        //std::cout << "snaps " << snaps[i] << std::endl;
		if( !next(trim(snaps[i]), 0, snap_folder)
			|| trim(snaps[i]).size()<=snap_folder.size() )
			continue;

		std::string stdout;
		std::string subvolume_folder = snaps[i];

		//std::cout << "******* subvolume_folder " << snaps[i] << std::endl;
		int rc=exec_wait(find_zfs_cmd(), stdout, "get", "-H", "-o", "value", "origin", subvolume_folder.c_str(), NULL);
		if(rc==0)
		{
			stdout=trim(stdout);
           // std::cout << "stdout :: "  << stdout << " snapshot is " << snapshot << std::endl;
			if(stdout==snapshot)
			{
				std::cout << "Origin is " << subvolume_folder << std::endl;


				if(exec_wait(find_zfs_cmd(), true, "promote", subvolume_folder.c_str(), NULL)!=0)
				{
					return false;
				}

				dependencies.push_back(subvolume_folder);
			}
		}
	}

	return true;
}

bool identify_dependencies(const std::string& snapshot, std::vector<std::string>& dependencies)
{
	std::cout << "Searching for dependencies for " << snapshot << std::endl;

	std::string snap_data;
	int rc = exec_wait(find_zfs_cmd(), snap_data, "get", "-H", "-o", "value", "clones" ,snapshot.c_str(), NULL);
	if(rc!=0)
		return false;

    std::cout << snap_data << std::endl;
	std::vector<std::string> snaps;
	Tokenize(snap_data, snaps, ",");

	for(size_t i=0;i<snaps.size();++i)
	{
      dependencies.push_back(snaps[i]);
	}
	return true;
}


bool delete_zfs_subvolume(int mode, std::string subvolume_folder, bool quiet=false)
{
	if(mode==mode_zfs)
	{
        std::string stdout;
        std::vector<std::string> dependencies;
        if(identify_dependencies((subvolume_folder+"@ro"),dependencies))
        {
           for(auto& d : dependencies)
           {
                std::cout << "dependency : " << d << std::endl;
           }
           if(dependencies.size() > 1)
           {
             return false;
           }
        }

		int rc = exec_wait(find_zfs_cmd(), false, "destroy", (subvolume_folder+"@ro").c_str(), NULL);
		std::cout << "destroy command result is " << stdout << "rc value " << rc << std::endl;
        rc = exec_wait(find_zfs_cmd(), false, "destroy", subvolume_folder.c_str(), NULL);
		if(rc!=0)
		{
            if(dependencies.size() > 0)
            {
                std::cout << "Promoting dependency " << dependencies[0] << std::endl;
                if(exec_wait(find_zfs_cmd(), true, "promote", dependencies[0].c_str(), NULL)!=0)
                {
                    return false;
                }

                int rc = exec_wait(find_zfs_cmd(), false, "destroy", (subvolume_folder+"@ro").c_str(), NULL);
                rc = exec_wait(find_zfs_cmd(), false, "destroy", subvolume_folder.c_str(), NULL);
                if(rc!=0)
                {
                   return false;
                }
			/*std::string rename_name = ExtractFileName(subvolume_folder);
			if(exec_wait(find_zfs_cmd(), true, "rename", (subvolume_folder+"@ro").c_str(), (subvolume_folder+"@"+rename_name).c_str(), NULL)!=0
				&& is_subvolume(mode, subvolume_folder+"@ro") )
			{
				return false;
			}

			std::vector<std::string> dependencies;
			if(!promote_dependencies(subvolume_folder+"@"+rename_name, dependencies))
			{
				return false;
			}


			if(!promote_dependencies(subvolume_folder+"@ro", dependencies))
			{
				return false;
			}

			for(auto& d : dependencies)
			{
                std::cout << "dependency : " << d << std::endl;
			}*/

			/*
			rc = exec_wait(find_zfs_cmd(), true, "destroy", subvolume_folder.c_str(), NULL);

			if(rc==0)
			{
				for(size_t i=0;i<dependencies.size();++i)
				{
					if(is_subvolume(mode, dependencies[i]+"@"+rename_name))
					{
						rc = exec_wait(find_zfs_cmd(), true, "destroy", (dependencies[i]+"@"+rename_name).c_str(), NULL);
						if(rc!=0)
						{
							break;
						}
					}
				}
			}*/
            }
        }
		return rc==0;
	}
	return false;
}


bool remove_subvolume(int mode, std::string subvolume_folder, bool quiet=false)
{
	if(mode==mode_zfs)
	{
        int rc = 0;
        std::vector<std::string> dependencies;
        if(is_subvolume(mode,subvolume_folder+"@ro"))
        {
            if(identify_dependencies((subvolume_folder+"@ro"),dependencies))
            {
               if(dependencies.size() > 1)
               {
                 return false;
               }
               else if(dependencies.size() == 1)
               {
                 if((dependencies[0].find("4096.virtual_machines") != std::string::npos) ||
                    (dependencies[0].find("4096.-flrmount") != std::string::npos)
                   )
                   {
                      return false;
                   }
               }
            }

            rc = exec_wait(find_zfs_cmd(), false, "destroy", (subvolume_folder+"@ro").c_str(), NULL);
		}
		//std::cout << "destroy command result is " << stdout << "rc value " << rc << std::endl;
        rc = exec_wait(find_zfs_cmd(), false, "destroy", subvolume_folder.c_str(), NULL);
		if(rc!=0)
		{
			std::string rename_name = ExtractFileName(subvolume_folder);
			if(exec_wait(find_zfs_cmd(), true, "rename", (subvolume_folder+"@ro").c_str(), (subvolume_folder+"@"+rename_name).c_str(), NULL)!=0
				&& is_subvolume(mode, subvolume_folder+"@ro") )
			{
				return false;
			}

			//std::string rename_name = "ro";
			std::vector<std::string> dependencies;
			if(!promote_dependencies(subvolume_folder+"@"+rename_name, dependencies))
			{
				return false;
			}


			rc = exec_wait(find_zfs_cmd(), true, "destroy", subvolume_folder.c_str(), NULL);

			if(rc==0)
			{
				for(size_t i=0;i<dependencies.size();++i)
				{
					if(is_subvolume(mode, dependencies[i]+"@"+rename_name))
					{
						rc = exec_wait(find_zfs_cmd(), true, "destroy", (dependencies[i]+"@"+rename_name).c_str(), NULL);
						if(rc!=0)
						{
							break;
						}
					}
				}
			}

        }
		return rc==0;
	}
	return false;
}

int zfs_test()
{
	std::cout << "Testing for zfs..." << std::endl;

	if(getBackupfolderPath(mode_zfs).empty())
	{
		std::cout << "TEST FAILED: Dataset is not set via /etc/urbackup/dataset" << std::endl;
		return 1;
	}

	std::string clientdir=getBackupfolderPath(mode_zfs)+os_file_sep()+"vmware/testA54hj5luZtlorr494";

	if(create_subvolume(mode_zfs, clientdir)
		&& remove_subvolume(mode_zfs, clientdir) )
	{
		std::cout << "ZFS TEST OK" << std::endl;

		clientdir=getBackupfolderPath(mode_zfs)+os_file_sep()+"vmware/testA54hj5luZtlorr494";

		if(getBackupfolderPath(mode_zfs).empty())
		{
			return 10 + mode_zfs;
		}
		else if(create_subvolume(mode_zfs, clientdir)
			&& remove_subvolume(mode_zfs, clientdir))
		{
			return 10 + mode_zfs;
		}

		return 10 + mode_zfs;
	}
	else
	{
		std::cout << "TEST FAILED: Creating test zfs volume \"" << clientdir << "\" failed" << std::endl;
	}
	return 1;
}

int import_zpool(const std::string& pool_name)
{
   int rc=exec_wait(find_zpool_cmd(), false, "import", "-f", pool_name.c_str(), NULL);
   return rc;
}

int mount_all_datasets()
{
   int rc=exec_wait(find_zfs_cmd(), false, "mount", "-a", NULL);
   return rc;
}

int check_data_set(const std::string& dataset_name, bool load_key)
{
  std::string values;
  int rc=exec_wait(find_zfs_cmd(), values, "get", "-H", "-o", "value", "mounted,encryption,keystatus", dataset_name.c_str(), NULL);
  if(rc==0)
  {
    if(load_key)
    {
        std::vector<std::string> properties;
        Tokenize(values, properties, "\n");

        std::string mounted = properties.at(0);
        std::string keystatus = properties.at(2);
        if(mounted == "no" && (keystatus.find("unavailable") != std::string::npos))
        {
             //Read key from the db...
             std::string enc_key;

             int rc=exec_wait(db_manager_path, enc_key, "read_zfs_pwd", NULL);
             if(rc==0)
             {
                enc_key = trim(enc_key);
             }
             else
             {
               std::cout << "Reading Enc key failed" << std::endl;
             }

            std::string strCmd = find_zfs_cmd() + " load-key " + dataset_name;
            std::cout << strCmd << std::endl;
            std::string args = enc_key;
            rc = exec_cmd_with_input(strCmd,args);
            if(rc != 0)
            {
               std::cout << "zfs key load failed " << std::endl;
            }
        }
    }
  }
  return rc;
}


int verify_dataset(const std::string& dataset_name)
{
  std::string values;
  int rc=exec_wait(find_zfs_cmd(), values, "get", "-H", "-o", "value", "mounted,encryption,keystatus", dataset_name.c_str(), NULL);
  if(rc==0)
  {
    std::vector<std::string> properties;
    Tokenize(values, properties, "\n");

    std::string mounted = properties.at(0);
    std::string encryption = properties.at(1);
    std::string keystatus = properties.at(2);
    if(mounted == "no")
    {
        std::cout << "dataset not mounted" << std::endl;
        rc = 1;
    }
    else if(encryption == "off")
    {
        std::cout << "key not set" << std::endl;
        rc = 2;
    }
    else if(keystatus != "available")
    {
        std::cout << "keystatus is not available" << std::endl;
        rc = 3;
    }
    else
    {
        rc = 0;
    }
  }
  else
  {
     std::cout << "dataset is not present" << std::endl;
     rc = 4;
  }
  return rc;
}


int init_zfs()
{
   std::string output;
   std::string pool_name = "idrivebmr";
   std::string data_set_images = pool_name + "/images";
   std::string data_set_vmware = pool_name + "/vmware";
   std::string data_set_bmrnas = pool_name + "/bmr_nas";

   int rc=exec_wait(find_zpool_cmd(), stdout, "list", "-H", "-o", "health", pool_name.c_str(), NULL);
   if(rc==0)
   {
		output=trim(output);

		if(output.find("ONLINE") != std::string::npos)
		{
          rc = import_zpool(pool_name);
          if(rc != 0)
          {
            std::cout << "Zpool idrivebmr could not be imported.. " << std::endl;
            return rc==0;
          }
		}
   }
   else
   {
      rc = import_zpool(pool_name);
      if(rc != 0)
      {
        std::cout << "Zpool idrivebmr could not be imported.. " << std::endl;
        return 1;
      }

      std::cout << "Zpool idrivebmr imported successfully " << std::endl;
   }

    rc = check_data_set(data_set_images, true);
    if(rc == 0)
    {
        rc = check_data_set(data_set_vmware, true);
        if(rc == 0)
        {
           rc = check_data_set(data_set_bmrnas, true);
            if(rc == 0)
            {
                 rc = mount_all_datasets();
                 if(rc == 0)
                 {
                    int rc1 = check_data_set(data_set_images, false);
                    int rc2 = check_data_set(data_set_vmware, false);
                    int rc3 = check_data_set(data_set_bmrnas, false);
                    if(rc1 == 0 && rc1 == 0 && rc3 == 0)
                    {
                        std::cout << "All data sets mounted successfully.. " << std::endl;
                        return 0;
                    }
                    else
                    {
                         std::cout << "Data sets mount after mount -a failed.. " << std::endl;
                         return 5;
                    }
                 }
                 else
                 {
                    return 4;
                 }
             }
             else
             {
                return 6;
             }
        }
        else
        {
            return 3;
        }
    }
    else
    {
        return 2;
    }
}

int main(int argc, char *argv[])
{
	if(argc<2)
	{
		std::cout << "Not enough parameters" << std::endl;
		return 1;
	}

	std::string cmd = argv[1];
    int mode=mode_zfs;

#ifndef _WIN32
	if(seteuid(0)!=0)
	{
		std::cout << "Cannot become root user" << std::endl;
		return 1;
	}
#endif

	if(cmd=="create")
	{
		if(argc<3)
		{
			std::cout << "Not enough parameters for create" << std::endl;
			return 1;
		}

		std::string subvolume_folder= argv[2];

		return create_subvolume(mode, subvolume_folder)?0:1;
	}
	else if(cmd=="mountpoint")
	{
		if(argc<3)
		{
			std::cout << "Not enough parameters for mountpoint" << std::endl;
			return 1;
		}

		std::string subvolume_folder=argv[2];

		return get_mountpoint(mode, subvolume_folder)?0:1;
	}
	else if(cmd=="snapshot")
	{
		if(argc<4)
		{
			std::cout << "Not enough parameters for snapshot" << std::endl;
			return 1;
		}

		std::string subvolume_src_folder= argv[2];
		std::string subvolume_dst_folder= argv[3];

		return create_snapshot(mode, subvolume_src_folder, subvolume_dst_folder)?0:1;
	}
	else if(cmd=="remove")
	{
		if(argc<3)
		{
			std::cout << "Not enough parameters for remove" << std::endl;
			return 1;
		}

		std::string subvolume_folder= argv[2];

		return remove_subvolume(mode, subvolume_folder)?0:1;
	}
	else if(cmd=="issubvolume")
	{
		if(argc<3)
		{
			std::cout << "Not enough parameters for issubvolume" << std::endl;
			return 1;
		}
		std::string subvolume_folder= argv[2];

		return is_subvolume(mode, subvolume_folder)?0:1;
	}
	else if(cmd=="makereadonly")
	{
		if(argc<3)
		{
			std::cout << "Not enough parameters for makereadonly" << std::endl;
			return 1;
		}

		std::string subvolume_folder= argv[2];

		return make_readonly(mode, subvolume_folder)?0:1;
	}
	else if(cmd=="makereadonlyex")
	{
		if(argc<4)
		{
			std::cout << "Not enough parameters for makereadonlyex" << std::endl;
			return 1;
		}

		std::string subvolume_folder= argv[2];
        std::string readonlyname = argv[3];
		return make_readonly_custom(mode, subvolume_folder,readonlyname)?0:1;
	}
	else if(cmd=="create-encrypt")
	{
		if(argc<3)
		{
			std::cout << "Not enough parameters for makereadonly" << std::endl;
			return 1;
		}

		std::string subvolume_folder= argv[2];
		std::string encryption_key = argv[3];

		return create_subvolume_with_encryption(subvolume_folder, encryption_key)?0:1;
	}
	else if(cmd=="init-zfs")
	{
		return init_zfs();
	}
	else if(cmd=="verify-dataset")
	{
        std::string dataset_name = argv[2];
		return verify_dataset(dataset_name);
	}
	else
	{
		std::cout << "Command not found" << std::endl;
		return 1;
	}
}

bool create_subvolume_with_encryption(std::string subvolume_folder, std::string encryption_key)
{
    FILE *f;

    if(is_subvolume(mode_zfs,subvolume_folder))
    {
       remove_subvolume(mode_zfs,subvolume_folder,true);
    }



    std::string createCmd = "sudo zfs create -o encryption=on -o keyformat=passphrase -o pbkdf2iters=1000000 " + subvolume_folder;
    f = popen (createCmd.c_str(), "w");
    if (!f)
    {
        perror ("popen");
        exit(1);
    }

    fprintf (f, "%s\n%s\n", encryption_key.c_str(), encryption_key.c_str());

    bool rc = pclose (f);
    return rc==0;
}
