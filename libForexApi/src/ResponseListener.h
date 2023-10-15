#ifndef RESPONSELISTENER_H
#define RESPONSELISTENER_H

#include "ForexConnect/ForexConnect.h"
#include "CriticalSection.h"

class CResponse
{
public:
	typedef enum {
		FAILED = -1,
		COMPLETED = 0,
	} ResponseStatus;

	CResponse(ResponseStatus responseStatus, IO2GResponse* response, const char* requestID, const char* error)
		: m_emResponseStatus(responseStatus) , m_pResponse(response) , m_sRequestID(requestID) , m_sError(error) {};
	~CResponse() {
		if (m_pResponse) {
			m_pResponse->release();
		}
	};

private:
	ResponseStatus m_emResponseStatus;
	IO2GResponse* m_pResponse;
	string m_sRequestID;
	string m_sError;

public:
	ResponseStatus getResponseStatus() const { return m_emResponseStatus; };
	IO2GResponse* getIResponse() const { return m_pResponse; };
	string getRequestID() const { return m_sRequestID; };
	string getError() const { return m_sError; };
};

class CResponseListener : public IO2GResponseListener
{
private:
	HANDLE m_hResponse;
	string m_sRequestID;
	queue<CResponse*> m_qeResponses;
	CCriticalSection m_csQueue;

public:
	CResponseListener();
	~CResponseListener();

	HANDLE getReponseEvent() const { return m_hResponse; };
	void setRequestID(const char* requestID);

	long addRef() { return 0; };
	long release() { return 0; };
	void onRequestCompleted(const char* requestID, IO2GResponse* response = 0);
	void onRequestFailed(const char* requestID , const char* error);
	void onTablesUpdates(IO2GResponse* data);

	void pushResponse(CResponse* response);
	CResponse* popResponse();

private:
	void clearResponseQueue();
};

#endif
