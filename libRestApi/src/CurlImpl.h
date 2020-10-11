#ifndef CURLIMPL_H
#define CURLIMPL_H

#include "curl/curl.h"

typedef struct {
	char *buf;
	size_t size;
} ResBuffer;
typedef void (*_curlResponseListener)(void* curlobj, void *listener);

typedef struct {
	string key;
	string value;
} ReqParam;

class CCurlImpl
{
private:
	CURL *m_pCurlHandle;
	bool m_bGetHeader;
	ResBuffer m_stResHeader;
	ResBuffer m_stResContents;
	_curlResponseListener m_fpResListener;
	struct curl_slist* m_pChunk;
	struct timeval m_tImplTime;

	bool m_bSslVerify;
	atomic<long> m_lRefreshInterval;
	string m_sHost;
	string m_sMethod;
	string m_sPath;
	string m_sReqString;
	string m_sFields;
	map<string, string> m_mapResFields;

public:
	static const char Blank[];
	static const char CurlAgent[];
	static const char ResTargetName[];

public:
	CCurlImpl(const char* host, bool sslVerify);
	~CCurlImpl();

	CURL* getCurlHandle() const;
	const char* getResHeader() const;
	const char* getResContents() const;
	const size_t getResSize() const;
	_curlResponseListener getResListener() const;
	string getUrl() const;
	string getResField(const char* key) const;

	CURLcode init(string method, string request, bool getHeader, _curlResponseListener listener = NULL);
	void setPath(map<string, string>& params);
	void setPath(const char* path, map<string, string>& params);
	void addHeader(const char* header);
	void addHeaders(vector<const char*>& headers);
	void parseResFileds(const char* response);
	void setRefreshInterval(long interval);
	bool chkRefresh();
	void clear();
	
	CURLcode setEasyPerform(vector<ReqParam>* params = NULL);
	CURLcode doEasyPerform();
	void onResListener(void* listener);
	string toString() const;

private:
	static size_t writeCallBack(void *contents, size_t size, size_t nmemb, void *buf);
};

#endif
