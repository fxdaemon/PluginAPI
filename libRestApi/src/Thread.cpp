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
#include "Thread.h"

CThread::CThread()
{
	m_stThreadFunAttr.m_fpThreadFun = NULL;
	m_hTerminateSignal = nsapi::CreateEvent(NULL, FALSE, FALSE, NULL);
	m_bIsStopRequested = false;
#ifdef WIN32
	m_hThread = 0;
#endif
	resetRunning();
}

CThread::CThread(ThreadFunAttr threadFunAttr) : m_stThreadFunAttr(threadFunAttr), m_bIsStopRequested(false)
{
	m_hTerminateSignal = nsapi::CreateEvent(NULL, FALSE, FALSE, NULL);
#ifdef WIN32
	m_hThread = 0;
#endif
	resetRunning();
}

CThread::~CThread()
{
	nsapi::CloseHandle(m_hTerminateSignal);
	if (!join(30000)) {
#ifdef WIN32
		CCriticalSection::Lock d(m_hLock);
		if (m_hThread) {
			::SuspendThread(m_hThread);
			::CloseHandle(m_hThread);
			m_hThread = 0;
			resetRunning();
		}
#else
		pthread_cancel(m_ptThread);
#endif
	}
}

bool CThread::isRunning() const
{
	CCriticalSection::Lock lock(m_hLock);
#ifdef WIN32
	return m_uiThreadID != 0; 
#else
	return m_bRunning;
#endif
}

void CThread::resetRunning()
{
#ifdef WIN32
	m_uiThreadID = 0;
#else
	m_bRunning = false;
#endif
}

bool CThread::isCurrentThread()
{
	CCriticalSection::Lock d(m_hLock);
#ifdef WIN32
	return m_uiThreadID == ::GetCurrentThreadId();
#else
	return pthread_equal(m_ptThread, pthread_self());
#endif
}

int CThread::_start()
{
	if (isRunning()) {
		return 0;
	}

	int ret = 0;
	{
		CCriticalSection::Lock d(m_hLock);
#ifdef WIN32
		if (m_hThread) {
			::CloseHandle(m_hThread);
		}
		m_hThread = (HANDLE)_beginthreadex(NULL, 0, _threadRunner, this, 0, &m_uiThreadID);
		if (m_hThread == (void *)-1L) {
			m_hThread = 0;
			resetRunning();
			ret = -1;
		}
#else
		if (pthread_create(&m_ptThread, NULL, &_threadRunner, this) == 0) {
			m_bRunning = true;
		} else {
			ret = -1;
		}
#endif
	}

	return ret;
}

bool CThread::join(unsigned long dwWaitMilliseconds)
{
	if (!isRunning()) {
		return true;
	}

#ifdef WIN32
	{
		CCriticalSection::Lock d(m_hLock);
		DWORD dwExitCode = 0;
		if (!GetExitCodeThread(m_hThread, &dwExitCode)) {
			return true;
		}
		if (dwExitCode != STILL_ACTIVE) {
			return true; // thread already terminated, so nothing to join.
		}
		if (m_uiThreadID == ::GetCurrentThreadId()) {
			return true;
		}
	}

	bool bRes = (::WaitForSingleObject(m_hThread, dwWaitMilliseconds) == WAIT_OBJECT_0);
	if (bRes) {
		CCriticalSection::Lock d(m_hLock);
		if (m_hThread) {
			::CloseHandle(m_hThread);
			m_hThread = 0;
		}
		m_uiThreadID = 0;
	}
	return bRes;
#else
	{
		CCriticalSection::Lock lock(m_hLock);
		if (!isStopRequested()) {
			requestStop();
		} else {
			return true;
		}
	}
	if (pthread_kill(m_ptThread, 0)) {
		return true; // thread already terminated
	}
	int iRes = pthread_join(m_ptThread, NULL);
	if (iRes == 0) {
		CCriticalSection::Lock lock(m_hLock);
		resetRunning();
		return true;
	} else if (iRes == EDEADLK) {
		return true;
	} else {
		return false;
	}
#endif
}

int CThread::terminate()
{
	resetRunning();
#ifdef WIN32
	return ::TerminateThread(m_hThread, 1);
#else
	return pthread_cancel(m_ptThread);
#endif
}

#ifdef WIN32
unsigned int WINAPI CThread::_threadRunner(void *pPtr)
#else
void *CThread::_threadRunner(void *pPtr)
#endif
{
	CThread *pObj = (CThread*)pPtr;
	if (pObj->m_stThreadFunAttr.m_fpThreadFun) {
		(pObj->m_stThreadFunAttr.m_fpThreadFun)(pObj->m_stThreadFunAttr.m_pVal);
	}
	pObj->run(pPtr);

#ifdef __linux__
	pthread_testcancel();
#endif

	{
		CCriticalSection::Lock d(pObj->m_hLock);
		pObj->m_bIsStopRequested = false;
		pObj->resetRunning();
	}

	nsapi::SetEvent(pObj->getTerminateSignal());
	return 0;
}

