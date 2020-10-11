#ifndef UTILS_H
#define UTILS_H

class CUtils
{
public:
	static long getTimeZone();
	static tm getUTCCal(time_t time);
	static time_t getUTCTime(tm* t);
	static time_t overDayoff(time_t start, int marketOpenWday, int marketOpenHour, int marketCloseWday, int marketCloseHour);
	static string strOfTime(tm* t, const char* format);
};

#endif
