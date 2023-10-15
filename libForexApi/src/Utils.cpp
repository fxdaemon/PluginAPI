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
	char buf[255];
	strftime(buf, sizeof(buf), format, t);
	return buf;
}
