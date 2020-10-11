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
	static string strOfTime(time_t t, const char* format);
	static string strOfTimeWithRFC3339(time_t t);
	static time_t str2Time(const char* s);
	static time_t HStr2Time(const char* s);
	static time_t getWeekFirstDate();
	static int getTimeOfDay(struct timeval *tv, struct timezone *tz);
	static void replace(string &s, const char *src, const char *dst);
};

#endif
