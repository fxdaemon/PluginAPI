#ifndef THREAD_H
#define THREAD_H

#include "CriticalSection.h"

typedef void (*_threadFun)(void*);
typedef struct {
	_threadFun m_fpThreadFun;
	void *m_pVal;
} ThreadFunAttr;

class CThread
{
protected:
#ifdef WIN32
	unsigned int m_uiThreadID;
	HANDLE m_hThread;
#else
	pthread_t m_ptThread;
	bool m_bRunning;
#endif
	mutable CCriticalSection m_hLock;
	ThreadFunAttr m_stThreadFunAttr;
	HANDLE m_hTerminateSignal;
	bool m_bIsStopRequested;

protected:
	void resetRunning();
	virtual int run(void *) { return 0; };
#ifdef WIN32
	static unsigned int WINAPI _threadRunner(void *);
#else
	static void *_threadRunner(void *);
#endif

public:
	CThread();
	CThread(ThreadFunAttr threadFunAttr);
	virtual ~CThread();

	void requestStop() { m_bIsStopRequested = true; }
	bool isStopRequested() const { return m_bIsStopRequested; }

	HANDLE getTerminateSignal() const { return m_hTerminateSignal; };
	bool isRunning() const;
	bool isCurrentThread();

	virtual int _start();
	virtual bool join(unsigned long dwWaitMilliseconds = INFINITE);	
	virtual int terminate();
};

#endif
