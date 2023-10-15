#ifndef CRITICALSECTION_H
#define CRITICALSECTION_H

class CCriticalSection
{
private:
#ifdef WIN32
	CRITICAL_SECTION m_oCritSection;
#else
	pthread_mutex_t m_oMutex;
#endif

public:
	CCriticalSection(bool recursive = true);
	~CCriticalSection();
	void lock();
	bool tryLock();
	void unlock();

	class Lock 
	{
	private:
		CCriticalSection* mutex;
	public:
		Lock(CCriticalSection& m) : mutex(&m) { mutex->lock();}
		Lock(CCriticalSection* m) : mutex(m) { mutex->lock(); }
		~Lock() { mutex->unlock(); }
	};
};

#endif
