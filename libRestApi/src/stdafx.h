#ifndef _STDAFX_H
#define _STDAFX_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cfloat>
#include <string>
#include <vector>
#include <queue>
#include <set>
#include <map>
#include <atomic>
using namespace std;

#ifdef WIN32

#define nsapi

#include <windows.h>
#include <process.h>
#pragma comment(lib, "ws2_32.lib")

struct timezone
{
	int  tz_minuteswest;
	int  tz_dsttime;
};
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

#else

#define nsapi gwin
#define GNUC

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  1
#endif
typedef unsigned long       DWORD, *LPDWORD;
typedef int                 BOOL, *LPBOOL;
typedef unsigned char       BYTE, *LPBYTE;
typedef unsigned short      WORD, *LPWORD;
typedef int                 *LPINT;
typedef long                LONG, *LPLONG, HRESULT;
typedef void                *LPVOID, *HINTERNET, *HANDLE, *HMODULE;
typedef const void          *LPCVOID;
typedef unsigned int        UINT, *PUINT;
typedef char                *LPSTR, *LPTSTR, _TCHAR;
typedef const char          *LPCSTR, *LPCTSTR;
typedef wchar_t             WCHAR, *PWCHAR, *LPWCH, *PWCH, *LPWSTR, *PWSTR;
typedef const WCHAR         *LPCWCH, *PCWCH, *LPCWSTR, *PCWSTR;

#define localtime_s(st, ltime) localtime_r(ltime, st)
#define gmtime_s(st, ltime) gmtime_r(ltime, st)
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define sprintf_s snprintf

#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <stdint.h>
#include <malloc.h>
#include <error.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include "WinEvent.h"

#endif

#endif
