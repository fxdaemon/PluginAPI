#ifndef WINEVENT_H
#define WINEVENT_H

#define SwitchToThread() sched_yield()
#define InterlockedBoolCompareExchange(A,B,C) __sync_bool_compare_and_swap(A,C,B)
#define InterlockedExchange(A,B) (long)__sync_lock_test_and_set(A,B)
#define InterlockedBoolExchange(A,B) (InterlockedExchange(A,B) == (B))
#define InterlockedIncrement(A) (long)__sync_add_and_fetch(A, 1L)
#define InterlockedDecrement(A) (long)__sync_sub_and_fetch(A, 1L)

#define CONST const
#define STATUS_WAIT_0		((DWORD)0x00000000L)
#define STATUS_TIMEOUT		((DWORD)0x00000102L)
#define WAIT_TIMEOUT		STATUS_TIMEOUT
#define WAIT_OBJECT_0		((STATUS_WAIT_0 ) + 0 )
#define CREATE_SUSPENDED	0x00000004
#define INFINITE			0xFFFFFFFF    // Infinite timeout
#define TIMEOUT_INF ~((unsigned)0u)

class CBaseHandle
{
public:
	CBaseHandle() {}
	virtual ~CBaseHandle() {}
	virtual bool wait(unsigned numMs) { return false; }
	virtual bool close() { return false; }
};

class CEventHandle : public CBaseHandle
{
public:
	CEventHandle(bool manualReset = false, bool signaled = false, const wchar_t* name = NULL);
	virtual ~CEventHandle();
	bool close();
	inline void incRefCount() { InterlockedIncrement(&m_RefCount); }
	inline int decRefCount() { InterlockedDecrement(&m_RefCount); return m_RefCount; }
	bool signaled() const { return m_Signaled; }
	std::wstring name() const { return m_Name; }
	void reset();
	void signal();
	bool pulse();
	bool wait(unsigned numMs);
	bool wait();
	void subscribe(CEventHandle* subscriber);
	void unSubscribe(CEventHandle* subscriber);
	bool isManualReset() const { return m_ManualReset; }
	void resetIfAuto();
private:
	pthread_mutex_t m_Mutex;
	pthread_mutex_t m_SubscrMutex;
	pthread_cond_t m_Cond;
	volatile bool m_ManualReset;
	volatile bool m_Signaled;
	volatile long m_Count;
	volatile long m_RefCount;
	std::wstring m_Name;
	std::set<CEventHandle*> m_Subscriber;
};

typedef struct _SECURITY_ATTRIBUTES
{
	BOOL bInheritHandle;
} SECURITY_ATTRIBUTES;
typedef SECURITY_ATTRIBUTES* LPSECURITY_ATTRIBUTES;

namespace gwin
{
	DWORD GetTickCount();
	void Sleep(DWORD dwMilliseconds);
	HANDLE CreateEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName);
	HANDLE OpenEvent(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
	BOOL CloseHandle(HANDLE hObject);
	BOOL SetEvent(HANDLE hEvent);
	BOOL ResetEvent(HANDLE hEvent);
	BOOL PulseEvent(HANDLE hEvent);
	DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
	DWORD WaitForMultipleObjects(DWORD nCount, CONST HANDLE *lpHandles, BOOL bWaitAll, DWORD dwMilliseconds);
}

#endif
