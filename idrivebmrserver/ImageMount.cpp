#include "ImageMount.h"
#include "../Interface/Server.h"
#include "dao/ServerBackupDao.h"
#include "database.h"
#include "../idrivebmrcommon/os_functions.h"
#include "../stringtools.h"
#include "server_cleanup.h"
#include <assert.h>
#ifdef _WIN32
#include <Windows.h>
#include <Subauth.h>
#include <imdisk.h>
bool os_link_symbolic_junctions_raw(const std::string &target, const std::string &lname, void* transaction);
#else
#include <errno.h>
#endif

std::map<int, size_t> ImageMount::mounted_images;
IMutex* ImageMount::mounted_images_mutex;
std::set<int> ImageMount::locked_images;
IMutex* ImageMount::mount_processes_mutex;
std::map<int, THREADPOOL_TICKET> ImageMount::mount_processes;

namespace
{
#ifdef _WIN32
	const std::string alt_mount_path = "C:\\IDriveBMRMounts";

	bool os_mount_image(const std::string& path, int backupid, std::string& errmsg)
	{
		if (path.size() <= 1)
		{
			return false;
		}

		std::wstring filename = Server->ConvertToWchar(path);

		std::vector<char> create_data_buf(sizeof(IMDISK_CREATE_DATA) + filename.size() * sizeof(wchar_t));
		IMDISK_CREATE_DATA* create_data = reinterpret_cast<IMDISK_CREATE_DATA*>(create_data_buf.data());

		create_data->FileNameLength = static_cast<USHORT>(filename.size() * sizeof(wchar_t));
		memcpy(create_data->FileName, filename.c_str(), create_data->FileNameLength);
		create_data->DeviceNumber = IMDISK_AUTO_DEVICE_NUMBER;
		create_data->Flags = IMDISK_TYPE_PROXY | IMDISK_PROXY_TYPE_TCP | IMDISK_OPTION_RO;


		HANDLE hDriver = CreateFile(IMDISK_CTL_DOSDEV_NAME,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		if (hDriver == INVALID_HANDLE_VALUE)
		{
			errmsg = "Error communicating with ImDisk driver. " + os_last_error_str();
			Server->Log(errmsg, LL_ERROR);
			return false;
		}

		DWORD bytes_returned;
		DWORD buffer_size = static_cast<DWORD>(create_data_buf.size());
		if (!DeviceIoControl(hDriver, IOCTL_IMDISK_CREATE_DEVICE,
			create_data, buffer_size, create_data,
			buffer_size, &bytes_returned, NULL))
		{
			errmsg = "Error creating ImDisk device. " + os_last_error_str();
			Server->Log(errmsg, LL_ERROR);
			return false;
		}

		CloseHandle(hDriver);

		std::string device_path = Server->ConvertFromWchar(IMDISK_DEVICE_BASE_NAME) +
			convert(static_cast<int64>(create_data->DeviceNumber)) + "\\";

		std::string mountpoint = ExtractFilePath(path) + os_file_sep() + "contents";

		if (!os_link_symbolic_junctions_raw(device_path, mountpoint, NULL))
		{
			Server->Log("Error creating junction on ImDisk mountpoint at \"" + mountpoint + "\". " + os_last_error_str(), LL_WARNING);

			mountpoint = alt_mount_path + os_file_sep() + convert(backupid);

			if (!((os_directory_exists(alt_mount_path) || os_create_dir_recursive(alt_mount_path))
				&& os_link_symbolic(device_path, mountpoint)))
			{
				errmsg = "Error creating junction on ImDisk mountpoint. " + os_last_error_str();
				Server->Log(errmsg, LL_ERROR);
				return false;
			}
		}

		int64 starttime = Server->getTimeMS();
		bool has_error = false;
		while (getFiles(mountpoint, &has_error).empty()
			&& has_error
			&& Server->getTimeMS() - starttime < 60 * 1000)
		{
			Server->wait(1000);
		}

		if (!getFiles(mountpoint, &has_error).empty()
			|| !has_error)
		{
			return true;
		}

		errmsg = "Timeout while mounting volume image";

		return false;
	}

	bool os_unmount_image(const std::string& mountpoint, const std::string& path, int backupid, std::string& errmsg)
	{
		std::string target;
		if (!os_get_symlink_target(mountpoint, target)
			|| target.empty())
		{
			errmsg = "Error getting mountpoint target. " + os_last_error_str();
			Server->Log(errmsg, LL_ERROR);
			return false;
		}

		if (target[target.size() - 1] == '\\')
		{
			target.erase(target.size() - 1, 1);
		}

		HANDLE hDevice = CreateFile(Server->ConvertToWchar("\\\\?\\GLOBALROOT" + target).c_str(),
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		if (hDevice == INVALID_HANDLE_VALUE)
		{
			errmsg = "Cannot open ImDisk device " + target + ". " + os_last_error_str();
			Server->Log(errmsg, LL_WARNING);
			os_remove_symlink_dir(mountpoint);
			return false;
		}

		DWORD bytes_returned;
		if (!DeviceIoControl(hDevice, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytes_returned, NULL))
		{
			Server->Log("Locking ImDisk device failed. " + os_last_error_str() + ". Forcing unmount...", LL_WARNING);
			DeviceIoControl(hDevice, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytes_returned, NULL);
			DeviceIoControl(hDevice, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytes_returned, NULL);
		}
		else
		{
			if (!DeviceIoControl(hDevice, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytes_returned, NULL))
			{
				Server->Log("Dismounting volume failed. " + os_last_error_str(), LL_ERROR);
			}
		}

		if (!DeviceIoControl(hDevice, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &bytes_returned, NULL))
		{
			Server->Log("Ejecting ImDisk failed. " + os_last_error_str() + ". Forcing removal...", LL_WARNING);

			std::string device_number = getafter(Server->ConvertFromWchar(IMDISK_DEVICE_BASE_NAME), target);
			HANDLE hDriver = CreateFile(IMDISK_CTL_DOSDEV_NAME,
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				0,
				NULL);

			if (hDriver == INVALID_HANDLE_VALUE)
			{
				CloseHandle(hDevice);
				errmsg = "Error communicating with ImDisk driver (2). " + os_last_error_str();
				Server->Log(errmsg, LL_ERROR);
				os_remove_symlink_dir(mountpoint);
				return false;
			}

			DWORD deviceNumber = watoi(device_number);
			if (!DeviceIoControl(hDriver, IOCTL_IMDISK_REMOVE_DEVICE,
				&deviceNumber, sizeof(deviceNumber), NULL,
				0, &bytes_returned, NULL))
			{
				errmsg = "Error removing ImDisk device. " + os_last_error_str();
				Server->Log(errmsg, LL_ERROR);
				CloseHandle(hDevice);
				CloseHandle(hDriver);
				os_remove_symlink_dir(mountpoint);
				return false;
			}

			CloseHandle(hDriver);
		}

		DeviceIoControl(hDevice, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytes_returned, NULL);
		CloseHandle(hDevice);

		return os_remove_symlink_dir(mountpoint);
	}
#else
	bool image_helper_action(const std::string& path, int backupid, const std::string& action, std::string& errmsg)
	{
		IDatabase* db = Server->getDatabase(Server->getThreadID(), IDRIVEBMRDB_SERVER);
		ServerBackupDao backup_dao(db);

		ServerBackupDao::CondString clientname = backup_dao.getClientnameByImageid(backupid);
		if (!clientname.exists)
		{
			return false;
		}

		std::string foldername = ExtractFileName(ExtractFilePath(path));
		std::string imagename = ExtractFileName(path);

		std::string mount_helper = Server->getServerParameter("mount_helper");
		if (mount_helper.empty())
		{
			mount_helper = "idrivebmr_mount_helper";
		}

		int rc = os_popen((mount_helper + " " + action + " \"" + clientname.value + "\" \"" + foldername + "\" \"" + imagename + "\" 2>&1").c_str(), errmsg);
		if (rc != 0)
		{
			Server->Log("Image mounting failed: "+errmsg, LL_ERROR);
		}
		return rc == 0;
	}

	bool os_mount_image(const std::string& path, int backupid, std::string& errmsg)
	{
		return image_helper_action(path, backupid, "mount", errmsg);
	}

	bool os_unmount_image(const std::string& mountpoint, const std::string& path, int backupid, std::string& errmsg)
	{
		return image_helper_action(path, backupid, "umount", errmsg);
	}
#endif


	class ScopedLockImage
	{
		int backupid;
		bool locked;

	public:
		ScopedLockImage(int backupid, int64 timeoutms)
			: backupid(backupid)
		{
			locked = ImageMount::lockImage(backupid, timeoutms);
		}

		~ScopedLockImage()
		{
			if (backupid != 0
				&& locked)
			{
				ImageMount::unlockImage(backupid);
			}
		}

		bool has_lock()
		{
			return locked;
		}
	};

	bool unmount_image(ServerBackupDao& backup_dao, int backupid, std::string& errmsg)
	{
		ServerBackupDao::SMountedImage image_inf = backup_dao.getMountedImage(backupid);
		if (!image_inf.exists)
		{
			return false;
		}

		std::string mountpoint = ExtractFilePath(image_inf.path) + os_file_sep() + "contents";

#ifdef _WIN32
		if (!os_directory_exists(mountpoint))
		{
			mountpoint = alt_mount_path + os_file_sep() + convert(backupid);
		}

		if (!os_directory_exists(mountpoint))
		{
			return false;
		}
#else
		if (!os_directory_exists(mountpoint) || errno == EACCES || errno == ENOTCONN)
		{
			mountpoint = ExtractFilePath(image_inf.path) + "_mnt";
		}

		if (!os_directory_exists(mountpoint) && errno != EACCES && errno != ENOTCONN)
		{
			return false;
		}
#endif

		return os_unmount_image(mountpoint, image_inf.path, backupid, errmsg);
	}
}

void ImageMount::operator()()
{
	mounted_images_mutex = Server->createMutex();
	mount_processes_mutex = Server->createMutex();

	Server->waitForStartupComplete();

	IDatabase* db = Server->getDatabase(Server->getThreadID(), IDRIVEBMRDB_SERVER);
	ServerBackupDao backup_dao(db);
	
	int waittimems = 0;
	int64 oldtimes = 0;
	while (true)
	{
		Server->wait(waittimems);

		std::vector<ServerBackupDao::SMountedImage> old_mounted_images =
			backup_dao.getOldMountedImages(oldtimes);

		waittimems = 60 * 1000;
		oldtimes = 60 * 60;

		{
			IScopedLock lock(mounted_images_mutex);
			for (size_t i = 0; i < old_mounted_images.size(); ++i)
			{
				if (mounted_images.find(old_mounted_images[i].id) == mounted_images.end()
					&& locked_images.find(old_mounted_images[i].id) == locked_images.end())
				{
					locked_images.insert(old_mounted_images[i].id);
					lock.relock(NULL);

					int64 passed_time_s = Server->getTimeSeconds() - old_mounted_images[i].mounttime;
					Server->Log("Unmounting mounted image backup id " + convert(old_mounted_images[i].id) +
						" path \"" + old_mounted_images[i].path + "\" mounted " + PrettyPrintTime(passed_time_s * 1000) + " ago", LL_INFO);
					std::string errmsg;
					if (!unmount_image(backup_dao, old_mounted_images[i].id, errmsg))
					{
						Server->Log("Unmounting image backup id " + convert(old_mounted_images[i].id) +
							" path \"" + old_mounted_images[i].path + "\" mounted " + PrettyPrintTime(passed_time_s * 1000)
							+ " ago failed: " + errmsg, LL_ERROR);
					}
					backup_dao.setImageUnmounted(old_mounted_images[i].id);

					lock.relock(mounted_images_mutex);
					locked_images.erase(locked_images.find(old_mounted_images[i].id));
				}
			}
		}

		{
			IScopedLock lock(mount_processes_mutex);
			for (std::map<int, THREADPOOL_TICKET>::iterator it = mount_processes.begin();
				it != mount_processes.end();)
			{
				std::map<int, THREADPOOL_TICKET>::iterator it_curr = it++;
				if (Server->getThreadPool()->waitFor(it_curr->second))
				{
					mount_processes.erase(it_curr);
				}
			}
		}
	}
}

bool ImageMount::mount_image(int backupid, ScopedMountedImage& mounted_image, int64 timeoutms, bool& has_timeout, std::string& errmsg)
{
	has_timeout = false;

	ScopedLockImage lock_image(backupid, timeoutms);
	if (!lock_image.has_lock())
	{
		has_timeout = true;
		return false;
	}
	return mount_image_int(backupid, mounted_image, timeoutms, has_timeout, errmsg);
}

namespace
{
	class MountImageThread : public IThread
	{
		int backupid;
		std::string& errmsg;
	public:
		MountImageThread(int backupid, std::string& errmsg)
			: backupid(backupid), errmsg(errmsg)
		{

		}

		void operator()()
		{
			ImageMount::mount_image_thread(backupid, errmsg);
			delete this;
		}
	};
}

bool ImageMount::mount_image_int(int backupid, ScopedMountedImage& mounted_image, 
	int64 timeoutms, bool& has_timeout, std::string& errmsg)
{
	IScopedLock lock(mount_processes_mutex);
	std::map<int, THREADPOOL_TICKET>::iterator it = mount_processes.find(backupid);
	THREADPOOL_TICKET ticket;
	if (it != mount_processes.end())
	{
		ticket = it->second;
		lock.relock(NULL);
	}
	else
	{
		ticket = Server->getThreadPool()->execute(new MountImageThread(backupid, errmsg), "mnt image");
		mount_processes.insert(std::make_pair(backupid, ticket));
		lock.relock(NULL);
	}

	if (Server->getThreadPool()->waitFor(ticket, static_cast<int>(timeoutms)))
	{
		IScopedLock lock(mount_processes_mutex);
		std::map<int, THREADPOOL_TICKET>::iterator it = mount_processes.find(backupid);
		if (it != mount_processes.end()
			&& it->second == ticket)
		{
			mount_processes.erase(it);
		}
		lock.relock(NULL);
		IDatabase* db = Server->getDatabase(Server->getThreadID(), IDRIVEBMRDB_SERVER);
		ServerBackupDao backup_dao(db);
		ServerBackupDao::SMountedImage image_inf = backup_dao.getMountedImage(backupid);
		if (image_inf.mounttime != 0)
		{
			mounted_image.reset(backupid);
		}
		return image_inf.mounttime!=0;
	}
	else
	{
		has_timeout = true;
		return false;
	}
}

std::string ImageMount::get_mount_path(int backupid, bool do_mount, ScopedMountedImage& mounted_image, 
	int64 timeoutms, bool& has_timeout, std::string& errmsg)
{
	has_timeout = false;

	ScopedLockImage lock_image(backupid, timeoutms);
	if (!lock_image.has_lock())
	{
		has_timeout = true;
		return std::string();
	}

	IDatabase* db = Server->getDatabase(Server->getThreadID(), IDRIVEBMRDB_SERVER);
	ServerBackupDao backup_dao(db);

	ServerBackupDao::SMountedImage image_inf = backup_dao.getMountedImage(backupid);
	if (!image_inf.exists)
	{
		return std::string();
	}

	bool has_mount_process = false;
	{
		IScopedLock lock(mount_processes_mutex);
		std::map<int, THREADPOOL_TICKET>::iterator it = mount_processes.find(backupid);
		has_mount_process = it != mount_processes.end();
	}

	if (image_inf.mounttime == 0
		|| has_mount_process)
	{
		if (do_mount)
		{
			if (!mount_image_int(backupid, mounted_image, timeoutms, has_timeout, errmsg))
			{
				return std::string();
			}
		}
		else
		{
			return std::string();
		}
	}

	std::string ret = ExtractFilePath(image_inf.path) + os_file_sep() + "contents";

	if (os_directory_exists(ret))
	{
		mounted_image.reset(backupid);
		backup_dao.setImageMounted(backupid);
		return ret;
	}

#ifdef _WIN32
	ret = alt_mount_path + os_file_sep() + convert(backupid);

	if (os_directory_exists(ret))
	{
		mounted_image.reset(backupid);
		backup_dao.setImageMounted(backupid);
		return ret;
	}
#else
	ret = ExtractFilePath(image_inf.path) + "_mnt";

	if (os_directory_exists(ret))
	{
		mounted_image.reset(backupid);
		backup_dao.setImageMounted(backupid);
		return ret;
	}
#endif

	return std::string();
}

void ImageMount::incrImageMounted(int backupid)
{
	IScopedLock lock(mounted_images_mutex);
	std::map<int, size_t>::iterator it = mounted_images.find(backupid);
	if (it == mounted_images.end())
	{
		ServerCleanupThread::lockImageFromCleanup(backupid);
	}

	++mounted_images[backupid];
}

void ImageMount::decrImageMounted(int backupid)
{
	IScopedLock lock(mounted_images_mutex);
	std::map<int, size_t>::iterator it = mounted_images.find(backupid);
	assert(it != mounted_images.end()
		&& it->second > 0);
	if (it == mounted_images.end())
	{
		return;
	}
	--it->second;
	if (it->second == 0)
	{
		mounted_images.erase(it);
		ServerCleanupThread::unlockImageFromCleanup(backupid);
	}
}

bool ImageMount::lockImage(int backupid, int64 timeoutms)
{
	int64 starttime = Server->getTimeMS();
	IScopedLock lock(mounted_images_mutex);
	while (locked_images.find(backupid) != locked_images.end())
	{
		lock.relock(NULL);

		if (timeoutms == 0)
			return false;

		Server->wait(100);

		if (timeoutms > 0
			&& Server->getTimeMS() - starttime > timeoutms)
		{
			return false;
		}

		lock.relock(mounted_images_mutex);
	}

	locked_images.insert(backupid);
	return true;
}

void ImageMount::unlockImage(int backupid)
{
	IScopedLock lock(mounted_images_mutex);
	locked_images.erase(locked_images.find(backupid));
}

void ImageMount::mount_image_thread(int backupid, std::string& errmsg)
{	
	IDatabase* db = Server->getDatabase(Server->getThreadID(), IDRIVEBMRDB_SERVER);
	ServerBackupDao backup_dao(db);

	ServerBackupDao::SMountedImage image_inf = backup_dao.getMountedImage(backupid);
	if (!image_inf.exists)
	{
		return;
	}

	backup_dao.setImageMounted(backupid);

	if (!os_mount_image(image_inf.path, backupid, errmsg))
	{
		backup_dao.setImageUnmounted(backupid);
		return;
	}
}
