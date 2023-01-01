#pragma once
class BackupAlert
{
public:

	void ScheduledBackupAlert();
	void run(std::function<void(void)> f, int duration);
	int getday();
	void foo();
	long long getscheduledstarttime();
};