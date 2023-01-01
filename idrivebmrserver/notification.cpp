#include <iostream>
#include <chrono>
#include <thread>
#include <functional>
#include <ctime>
#include <string>
#include "server_settings.h"
#include "ImageBackup.h"
#include "Backup.h"
#include "ClientMain.h"
#include "notification .h"
using namespace std;

ServerSettings *settings;
std::string stmp = settings->getBackupWindowIncrImagestring();

ImageBackup *backup;
Backup *mail;
BackupAlert *alert;
void  BackupAlert::ScheduledBackupAlert()
{
	run(alert->foo, 5000);
	while (true);	
}
void BackupAlert::run(std::function<void(void)> f, int duration)
{
	std::thread([f, duration]() {
		while (true)
		{
			if (alert->getscheduledstarttime <= Server->getTimeMS+180000)
			{
				f();
				std::this_thread::sleep_for(180000);
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(duration));
			}
		}
	}).detach();
}
int BackupAlert::getday()
{
	time_t now = time(0);

	// convert now to string form
	char* date_time = ctime(&now);

	string res(date_time);
	string out = res.substr(0,3);
	
	switch (out[0]) {
	case 'M':
		return 1;
		break;
	case 'T':
		switch (out[1])
		{
		case 'u':
			return 2;
			break;
		default:
			return 4;
			break;
		}
		break;
	case 'W':
		return 3;
		break;
	case 'F':
		return 5;
		break;
	case 'S':
		switch (out[1])
		{
		case 'u':
			return 7;
			break;
		default:
			return 6;
			break;
		}
		break;
	default:
		return -1;
		break;
	}
	return -1;
}
long long BackupAlert::getscheduledstarttime()
{
	long long start_time = stmp[5] * 3600000;
	return start_time;
}

void BackupAlert::foo() {
	//"1-7/0-24"
	// get today date
	int day = alert->getday();

	// check date -1 is in range
	int start_day = stmp[1] - '0';
	int end_day = stmp[3] - '0';
	if (day == 1)
	{
		day += 7;
	}
	if ((day - 1) >= start_day && (day - 1) <= end_day)
	{
		if (!(backup->isbackuptriggered))//check is backup is not triggered
		{
			mail->sendschedulebackupalertMail();
		}
		else
		{
			backuptriggered = false;
		}
	}


}

