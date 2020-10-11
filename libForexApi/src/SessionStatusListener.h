#ifndef SESSIONSTUTUSLISTENER_H
#define SESSIONSTUTUSLISTENER_H

#include "ForexConnect/ForexConnect.h"
#include "CriticalSection.h"
#include "IPluginProxy.h"

class CLoginDataProvider
{
private:
	string m_sSessionID;
	string m_sPin;
public:
	CLoginDataProvider(const char* sessionID, const char* pin) : m_sSessionID(sessionID) , m_sPin(pin) {};
	string getSessionID() const { return m_sSessionID; };
	string getPin() const { return m_sPin; };
};


class CSessionStatusListener : public IO2GSessionStatus
{
private:
	HANDLE m_hSyncEvent;
	HANDLE m_hStopEvent;
	HANDLE m_hConnectedEvent;
	
	IO2GSession* m_pSession;
	CLoginDataProvider* m_pLoginDataProvider;

	CCriticalSection m_csStatusInfo;
	IPluginProxy *m_pPluginProxy;

public:
	CSessionStatusListener(IO2GSession* session, CLoginDataProvider* loginDataProvider, IPluginProxy *pluginProxy);
	~CSessionStatusListener();

	HANDLE getStatusEvent() const { return m_hSyncEvent; };
	HANDLE getStopEvent() const { return m_hStopEvent; };
	HANDLE getConnectedEvent() const { return m_hConnectedEvent; };
	
	long addRef() { return 0; };
	long release() { return 0; };
	void onSessionStatusChanged(O2GSessionStatus status);
	void onLoginFailed(const char* error);
};

#endif
