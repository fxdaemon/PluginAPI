/* Copyright 2011 Forex Capital Markets LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use these files except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "stdafx.h"
#include "CriticalSection.h"

CCriticalSection::CCriticalSection(bool recursive)
{
#ifdef WIN32
	::InitializeCriticalSection(&m_oCritSection);
#else
	if (recursive){
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&m_oMutex, &attr);
		pthread_mutexattr_destroy(&attr);
	} else {
		pthread_mutex_init(&m_oMutex, 0);
	}
#endif
}

CCriticalSection::~CCriticalSection()
{
#ifdef WIN32
	::DeleteCriticalSection(&m_oCritSection);
#else
	pthread_mutex_destroy(&m_oMutex);
#endif
}

void CCriticalSection::lock() 
{
#ifdef WIN32
	::EnterCriticalSection(&m_oCritSection);
#else
	pthread_mutex_lock(&m_oMutex);
#endif
}

bool CCriticalSection::tryLock() 
{
#ifdef WIN32
	return ::TryEnterCriticalSection(&m_oCritSection) == 0 ? false : true;
#else
	return pthread_mutex_trylock(&m_oMutex) == 0 ? true : false;
#endif
}

void CCriticalSection::unlock()
{
#ifdef WIN32
	::LeaveCriticalSection(&m_oCritSection);
#else
	pthread_mutex_unlock(&m_oMutex);
#endif
}
