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

#ifdef GNUC

static CCriticalSection s_CritSect;

timespec* getTimeout(struct timespec* spec, unsigned numMs)
{
	struct timeb currSysTime;
	ftime(&currSysTime);

	unsigned long nMsNew = numMs + currSysTime.millitm;

	spec->tv_sec = long(currSysTime.time) + (nMsNew / 1000);
	spec->tv_nsec = (nMsNew % 1000) * 1000000;
	return spec;
}

CEventHandle::CEventHandle(bool manualReset, bool signaled, const wchar_t* name)
	: CBaseHandle()
	, m_ManualReset(manualReset)
	, m_Signaled(signaled)
	, m_Count(0)
	, m_RefCount(1)
	, m_Name(name == NULL ? L"" : name)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutex_init(&m_Mutex, &attr);
	pthread_mutex_init(&m_SubscrMutex, &attr);
	pthread_mutexattr_destroy(&attr);
	pthread_cond_init(&m_Cond, NULL);
}

CEventHandle::~CEventHandle()
{
	pthread_cond_destroy(&m_Cond);
	pthread_mutex_destroy(&m_Mutex);
	pthread_mutex_destroy(&m_SubscrMutex);
}

void CEventHandle::signal()
{
	pthread_mutex_lock(&m_Mutex);
	m_Signaled = true;
	InterlockedIncrement(&m_Count);
	pthread_cond_broadcast(&m_Cond);
	pthread_mutex_unlock(&m_Mutex);//
								   // signal all subscribers (used in WaitForMultipleObjects())
	pthread_mutex_lock(&m_SubscrMutex);
	for (std::set<CEventHandle*>::iterator iter(m_Subscriber.begin()); iter != m_Subscriber.end(); iter++) {
		(*iter)->signal();
	}
	pthread_mutex_unlock(&m_SubscrMutex);
}

bool CEventHandle::pulse()
{
	// Only used for shutdown. ToDo !
	signal();
	return true;
}

void CEventHandle::reset()
{
	pthread_mutex_lock(&m_Mutex);
	m_Signaled = false;
	pthread_mutex_unlock(&m_Mutex);
}

bool CEventHandle::wait()
{
	return wait(TIMEOUT_INF);
}

bool CEventHandle::wait(unsigned numMs)
{
	int rc(0);
	struct timespec spec;
	pthread_mutex_lock(&m_Mutex);
	const long count(m_Count);
	while (!m_Signaled && m_Count == count) {
		if (numMs != TIMEOUT_INF) {
			rc = pthread_cond_timedwait(&m_Cond, &m_Mutex, getTimeout(&spec, numMs));
		}
		else {
			pthread_cond_wait(&m_Cond, &m_Mutex);
		}
		if (rc == ETIMEDOUT) {
			break;
		}
	}
	if (!m_ManualReset) {
		// autoReset
		m_Signaled = false;
	}
	pthread_mutex_unlock(&m_Mutex);
	return rc != ETIMEDOUT;
}

//Stores subscriber event to signal instead of signalling "this".
void CEventHandle::subscribe(CEventHandle* subscriber)
{
	pthread_mutex_lock(&m_SubscrMutex);
	m_Subscriber.insert(subscriber);
	pthread_mutex_unlock(&m_SubscrMutex);
}

//Removes a subscriber event from the subscriber set (external critical section).
void CEventHandle::unSubscribe(CEventHandle* subscriber)
{
	pthread_mutex_lock(&m_SubscrMutex);

	std::set<CEventHandle*>::iterator element = m_Subscriber.find(subscriber);
	if (element != m_Subscriber.end()) {
		m_Subscriber.erase(element);
	}

	pthread_mutex_unlock(&m_SubscrMutex);
}

//Performs auto reset.
void CEventHandle::resetIfAuto()
{
	if (!m_ManualReset) {
		reset();
	}
}

bool CEventHandle::close()
{
	if (decRefCount() <= 0) {
		delete this;
	}
	return true;
}

inline CEventHandle* castToWinEventHandle(HANDLE hEvent)
{
	return (CEventHandle *)(hEvent);
	// the cast below causes unexpected crashes due to 0 returned in several environments (android build, and some mac's (with gcc 4.1.2 and -O2))
	//return dynamic_cast<CEventHandle*>( reinterpret_cast<CBaseHandle*>(hEvent) );
}


namespace gwin
{

	DWORD GetTickCount()
	{
		struct timeb currSysTime;
		ftime(&currSysTime);
		return long(currSysTime.time) * 1000 + currSysTime.millitm;
	}

	void Sleep(DWORD dwMilliseconds)
	{
		usleep(1000 * dwMilliseconds);
	}

	HANDLE CreateEvent(LPSECURITY_ATTRIBUTES, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName)
	{
		CEventHandle* handle(new CEventHandle(bManualReset != FALSE, bInitialState != FALSE, lpName));
		return handle;
	}

	HANDLE OpenEvent(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
	{
		return NULL;
	}

	BOOL CloseHandle(HANDLE handle)
	{
		bool ret(false);
		if (handle != NULL) {
			CBaseHandle* baseHandle(static_cast<CBaseHandle*>(handle));
			if (!baseHandle->close()) {
				printf("Closing unknown HANDLE type\n");
			}
			ret = true;
		}
		return ret;
	}

	BOOL SetEvent(HANDLE hEvent)
	{
		CCriticalSection::Lock l(s_CritSect); //The lock avoids a race condition with subscribe() in WaitForMultipleObjects()
		castToWinEventHandle(hEvent)->signal();
		return true;
	}

	BOOL ResetEvent(HANDLE hEvent)
	{
		castToWinEventHandle(hEvent)->reset();
		return true;
	}

	BOOL PulseEvent(HANDLE hEvent)
	{
		return castToWinEventHandle(hEvent)->pulse();
	}

	DWORD WaitForSingleObject(HANDLE obj, DWORD timeMs)
	{
		CBaseHandle* handle(static_cast<CBaseHandle*>(obj));
		if (handle->wait(timeMs)) {
			return WAIT_OBJECT_0;
		}
		// Might be handle of wrong type?
		return WAIT_TIMEOUT;
	}

	DWORD WaitForMultipleObjects(DWORD numObj, const HANDLE* objs, BOOL waitAll, DWORD timeMs)
	{
		static const DWORD MAXIMUM_WAIT_OBJECTS(32);
		CEventHandle* eventHandle[MAXIMUM_WAIT_OBJECTS];
		//assert(numObj <= MAXIMUM_WAIT_OBJECTS);
		if (waitAll) {
			const DWORD startMs(GetTickCount());
			for (unsigned ix(0); ix < numObj; ix++) {
				// Wait for all events, one after the other.
				CEventHandle* event(castToWinEventHandle(objs[ix]));
				//assert(event);
				DWORD usedMs(GetTickCount() - startMs);
				if (usedMs > timeMs) {
					return WAIT_TIMEOUT;
				}
				if (!event->wait(timeMs - usedMs)) {
					return WAIT_TIMEOUT;
				}
			}
			return WAIT_OBJECT_0;
		}
		s_CritSect.lock();//
						  // Check whether any event is already signaled
		for (unsigned ix(0); ix < numObj; ix++) {
			CEventHandle* event(castToWinEventHandle(objs[ix]));
			//assert(event);
			if (event->signaled()) {
				event->resetIfAuto(); // Added 13.09.2008 (bug detected by BRAD H)
				s_CritSect.unlock();//
				return ix;
			}
			eventHandle[ix] = event;
		}
		if (timeMs == 0) {
			// Only check, do not wait. Has already failed, see above.
			s_CritSect.unlock();//
			return WAIT_TIMEOUT;
		}

		/***************************************************************************
		Main use case: No event signaled yet, create a subscriber event.
		***************************************************************************/
		CEventHandle subscriberEvent(false, 0);
		// Subscribe at the original objects
		for (unsigned ix(0); ix < numObj; ix++) {
			eventHandle[ix]->subscribe(&subscriberEvent);
		}
		s_CritSect.unlock(); // Re-enables SetEvent(). OK since the subscription is done

		bool success(subscriberEvent.wait(timeMs));

		// Unsubscribe and determine return value
		DWORD ret(WAIT_TIMEOUT);
		s_CritSect.lock();//
		for (unsigned ix(0); ix < numObj; ix++) {
			if (success && eventHandle[ix]->signaled()) {
				success = false;
				ret = ix;
				// Reset event that terminated the WaitForMultipleObjects() call
				eventHandle[ix]->resetIfAuto(); // Added 16.09.2009 (Alessandro)
			}
			eventHandle[ix]->unSubscribe(&subscriberEvent);
		}
		s_CritSect.unlock();
		return ret;
	}

}

#endif