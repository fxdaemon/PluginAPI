/*
* Copyright 2020 FXDaemon
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include "stdafx.h"
#include "Utils.h"

static const char* MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

long CUtils::getTimeZone()
{
#ifdef WIN32
	TIME_ZONE_INFORMATION timeZoneInfo;
	DWORD dwRet = ::GetTimeZoneInformation(&timeZoneInfo);
	if (dwRet == TIME_ZONE_ID_STANDARD || dwRet == TIME_ZONE_ID_UNKNOWN) {
		return timeZoneInfo.Bias * 60;
	}
	return 0;
#else
	time_t t = time(NULL);
	struct tm lt = { 0 };
	localtime_r(&t, &lt);
	return -lt.tm_gmtoff;
#endif
}

tm CUtils::getUTCCal(time_t time)
{
	tm tmCal;
	memset(&tmCal, 0, sizeof(tmCal));
	gmtime_s(&tmCal, &time);
	return tmCal;
}

time_t CUtils::getUTCTime(tm* t)
{
	return mktime(t) - getTimeZone();
}

time_t CUtils::overDayoff(time_t start, int marketOpenWday, int marketOpenHour, int marketCloseWday, int marketCloseHour)
{
	time_t next = start;
	tm dtStart = getUTCCal(next);
	while ((dtStart.tm_wday == marketCloseWday && dtStart.tm_hour > marketCloseHour) ||
		dtStart.tm_wday == 0 ||
		(dtStart.tm_wday == marketOpenWday && dtStart.tm_hour < marketOpenHour)) {
		next += 60;
		dtStart = getUTCCal(next);
	}

	if (next == start) {
		next += 3600;	// 1 hour
	}

	return next;
}

string CUtils::strOfTime(tm* t, const char* format)
{
	char buf[256];
	strftime(buf, sizeof(buf), format, t);
	return buf;
}

string CUtils::strOfTime(time_t t, const char* format)
{
	tm tmCal = getUTCCal(t);
	return strOfTime(&tmCal, format);
}

string CUtils::strOfTimeWithRFC3339(time_t t)
{
	tm tmCal = getUTCCal(t);
	return strOfTime(&tmCal, "%Y-%m-%dT%H:%M:%S");
}

time_t CUtils::str2Time(const char* s)
{
	struct tm tmCal;
	memset(&tmCal, 0, sizeof(tmCal));

	int ret =sscanf(s, "%d-%d-%dT%d:%d:%d",
		&tmCal.tm_year, &tmCal.tm_mon, &tmCal.tm_mday,
		&tmCal.tm_hour, &tmCal.tm_min, &tmCal.tm_sec);
	if (ret != 6) {
		ret = sscanf(s, "%d-%d-%d %d:%d:%d",
			&tmCal.tm_year, &tmCal.tm_mon, &tmCal.tm_mday,
			&tmCal.tm_hour, &tmCal.tm_min, &tmCal.tm_sec);
	}
	if (ret != 6) {
		ret = sscanf(s, "%d/%d/%d %d:%d:%d",
			&tmCal.tm_year, &tmCal.tm_mon, &tmCal.tm_mday,
			&tmCal.tm_hour, &tmCal.tm_min, &tmCal.tm_sec);
	}
	if (ret != 6) {
		return 0;
	}
	
	tmCal.tm_year -= 1900;
	tmCal.tm_mon -= 1;

	time_t t = mktime(&tmCal);
	t -= getTimeZone();
	return t;
}

// format: "Date: Tue, 27 Dec 2016 13:25:49 GMT"
time_t CUtils::HStr2Time(const char* s)
{
	tm tmCal;
	char s1[256], s2[256];

	string dstr = s;
	string::size_type pos = dstr.find("Date: ");
	if (pos != string::npos) {
		dstr = dstr.substr(pos);
	}

	memset(&tmCal, 0, sizeof(tmCal));
	if (sscanf(dstr.c_str(), "Date: %s %d %s %d %d:%d:%d",
		s1, &tmCal.tm_mday, s2, &tmCal.tm_year,
		&tmCal.tm_hour, &tmCal.tm_min, &tmCal.tm_sec) != 7) {
		return 0;
	}
	tmCal.tm_year -= 1900;
	for (unsigned int i = 0; i < sizeof(MONTHS) / sizeof(MONTHS[0]); i++) {
		if (strcmp(MONTHS[i], s2) == 0) {
			tmCal.tm_mon = i;
			break;
		}
	}

	time_t t = mktime(&tmCal);
	t -= getTimeZone();
	return t;
}

time_t CUtils::getWeekFirstDate()
{
	time_t t;
	tm tmNow, tmWeek;

	time(&t);
	localtime_s(&tmNow, &t);

	memset(&tmWeek, 0, sizeof(tmWeek));
	tmWeek.tm_year = tmNow.tm_year;
	tmWeek.tm_mon = tmNow.tm_mon;
	tmWeek.tm_mday = tmNow.tm_mday;

	return mktime(&tmWeek) - tmNow.tm_wday * 24 * 3600;
}

int CUtils::getTimeOfDay(struct timeval *tv, struct timezone *tz)
{
#ifdef WIN32
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag = 0;
	if (NULL != tv) {
		GetSystemTimeAsFileTime(&ft);
		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;
		tmpres /= 10;
		tmpres -= DELTA_EPOCH_IN_MICROSECS;
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}
	if (NULL != tz) {
		if (!tzflag) {
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}
	return 0;
#else
	return gettimeofday(tv, tz);
#endif
}

void CUtils::replace(string &s, const char *src, const char *dst)
{
	string::size_type pos = 0;
	string::size_type srclen = strlen(src);
	string::size_type dstlen = strlen(dst);
	while ((pos = s.find(src, pos)) != string::npos) {
		s.replace(pos, srclen, dst);
		pos += dstlen;
	}
}
