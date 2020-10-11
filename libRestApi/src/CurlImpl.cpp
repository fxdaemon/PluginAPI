/*
* Copyright 2020 FXDaemon
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include "stdafx.h"
#include "Utils.h"
#include "CurlImpl.h"

const char CCurlImpl::Blank[] = "";
const char CCurlImpl::CurlAgent[] = "RestApi-Plugin/1.0";
const char CCurlImpl::ResTargetName[] = "JsonResTargetName";

CCurlImpl::CCurlImpl(const char* host, bool sslVerify)
{
	m_pCurlHandle = NULL;
	m_bGetHeader = false;
	m_stResHeader = { NULL, 0 };
	m_stResContents = { NULL, 0 };
	m_fpResListener = NULL;
	m_pChunk = NULL;
	m_tImplTime = { 0, 0 };
	m_bSslVerify = false;
	m_lRefreshInterval.store(0);

	m_sHost.assign(host);
	m_bSslVerify = sslVerify;
}

CCurlImpl::~CCurlImpl()
{
	if (m_pCurlHandle) {
		curl_easy_cleanup(m_pCurlHandle);
	}
	clear();
	
	if (m_pChunk) {
		curl_slist_free_all(m_pChunk);
	}
}

CURL* CCurlImpl::getCurlHandle() const
{
	return m_pCurlHandle;
}

const char* CCurlImpl::getResHeader() const
{
	return m_stResHeader.buf ? m_stResHeader.buf : Blank;
}

const char* CCurlImpl::getResContents() const
{
	return m_stResContents.buf ? m_stResContents.buf : Blank;
}

const size_t CCurlImpl::getResSize() const
{
	return m_stResContents.buf ? m_stResContents.size : 0;
}

_curlResponseListener CCurlImpl::getResListener() const
{
	return m_fpResListener;
}

string CCurlImpl::getUrl() const
{
	char* url = NULL;
	curl_easy_getinfo(m_pCurlHandle, CURLINFO_EFFECTIVE_URL, &url);
	if (url) {
		return url;
	}
	return Blank;
}

string CCurlImpl::getResField(const char* key) const
{
	map<string, string>::const_iterator pos = m_mapResFields.find(key);
	if (pos == m_mapResFields.end()) {
		return Blank;
	}
	return pos->second;
}

CURLcode CCurlImpl::init(string method, string request, bool getHeader, _curlResponseListener listener)
{
	m_pCurlHandle = curl_easy_init();
	if (!m_pCurlHandle) {
		return CURLE_FAILED_INIT;
	}

	m_sMethod.assign(method);
	m_sReqString.assign(request);
	m_bGetHeader = getHeader;
	m_fpResListener = listener;
	CUtils::getTimeOfDay(&m_tImplTime, NULL);

	if (m_bGetHeader) {
		curl_easy_setopt(m_pCurlHandle, CURLOPT_HEADERFUNCTION, writeCallBack);
		curl_easy_setopt(m_pCurlHandle, CURLOPT_HEADERDATA, &m_stResHeader);
	}
	curl_easy_setopt(m_pCurlHandle, CURLOPT_WRITEFUNCTION, writeCallBack);
	curl_easy_setopt(m_pCurlHandle, CURLOPT_WRITEDATA, &m_stResContents);
	curl_easy_setopt(m_pCurlHandle, CURLOPT_USERAGENT, CurlAgent);
	if (m_pChunk) {
		curl_easy_setopt(m_pCurlHandle, CURLOPT_HTTPHEADER, m_pChunk);
	}
	if (m_sHost.substr(0, 5) == "https" && !m_bSslVerify) {
		curl_easy_setopt(m_pCurlHandle, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(m_pCurlHandle, CURLOPT_SSL_VERIFYHOST, 0L);
	}

	return CURLE_OK;
}

void CCurlImpl::setPath(map<string, string>& params)
{
	map<string, string>::const_iterator mpos = params.begin();
	while (mpos != params.end()) {
		CUtils::replace(m_sPath, mpos->first.c_str(), mpos->second.c_str());
		mpos++;
	}
}

void CCurlImpl::setPath(const char* path, map<string, string>& params)
{
	m_sPath.assign(path);
	setPath(params);
}

void CCurlImpl::addHeader(const char* header)
{
	m_pChunk = curl_slist_append(m_pChunk, header);
}

void CCurlImpl::addHeaders(vector<const char*>& headers)
{
	for (int i = 0; i < headers.size(); i++) {
		addHeader(headers[i]);
	}
}

void CCurlImpl::parseResFileds(const char* response)
{
	char* s = strdup(response);
	int len = strlen(response);
	
	m_mapResFields.insert(pair<string, string>(ResTargetName, Blank));

	int i = 0, colon = 0;
	while (s[i]) {
		if (s[i] == ':') {
			s[i] = '\0';
			m_mapResFields[ResTargetName] = s;
			colon = i + 1;
			break;
		}
		i++;
	}

	i = colon;
	while (s[i]) {
		if (s[i] == ',') {
			s[i] = '\0';
		}
		i++;
	}

	char *key, *value, *p = s + colon;
	for (i = colon; i < len; i++) {
		if (s[i] == '-') {
			s[i] = '\0';
			key = p;
			value = s + i + 1;
			m_mapResFields.insert(pair<string, string>(key, value));
			i += (strlen(value) + 1);
			p += (strlen(key) + 1 + strlen(value) + 1);
		}
	}

	free(s);
}

void CCurlImpl::setRefreshInterval(long interval)
{
	m_lRefreshInterval.store(interval);
}

bool CCurlImpl::chkRefresh()
{
	long interval = m_lRefreshInterval.load();
	if (interval > 0) {
		struct timeval tv;
		CUtils::getTimeOfDay(&tv, NULL);
		if ((tv.tv_sec - m_tImplTime.tv_sec) * 1000 + (tv.tv_usec - m_tImplTime.tv_usec) / 1000 > interval) {
			m_tImplTime = tv;
			return true;
		}
	}
	return false;
}

void CCurlImpl::clear()
{
	if (m_stResHeader.buf) {
		free(m_stResHeader.buf);
		m_stResHeader.buf = NULL;
	}
	if (m_stResContents.buf) {
		free(m_stResContents.buf);
		m_stResContents.buf = NULL;
	}
}

CURLcode CCurlImpl::setEasyPerform(vector<ReqParam>* params)
{
	CURLcode ret = CURLE_OK;

	m_sFields.assign(m_sReqString.c_str());
	if (params) {
		vector<ReqParam>::const_iterator pos = params->begin();
		while (pos != params->end()) {
			CUtils::replace(m_sFields, (*pos).key.c_str(), (*pos).value.c_str());
			pos++;
		}
	}

	string url = m_sHost;
	url.append(m_sPath);
	if (m_sMethod == "GET") {
		if (!m_sFields.empty()) {
			url.append("?").append(m_sFields);
		}		
	} else {
		if (m_sMethod != "POST") {
			ret = curl_easy_setopt(m_pCurlHandle, CURLOPT_CUSTOMREQUEST, m_sMethod.c_str());
		}
		if (!m_sFields.empty()) {
			ret = curl_easy_setopt(m_pCurlHandle, CURLOPT_POSTFIELDSIZE, m_sFields.size());
			ret = curl_easy_setopt(m_pCurlHandle, CURLOPT_POSTFIELDS, m_sFields.c_str());
		}

	}
	ret = curl_easy_setopt(m_pCurlHandle, CURLOPT_URL, url.c_str());

	return ret;
}

CURLcode CCurlImpl::doEasyPerform()
{
	clear();
	return curl_easy_perform(m_pCurlHandle);
}

void CCurlImpl::onResListener(void* listener)
{
	m_fpResListener((void*)this, listener);
}

string CCurlImpl::toString() const
{
	string s = "\nMethod: " + m_sMethod;
	s = s + "\nUrl: " + getUrl();
	s = s + "\nFields: " + m_sFields;
	s = s + "\nChunk: ";
	struct curl_slist* chunk = m_pChunk;
	while (chunk) {
		s = s + string(chunk->data);
		chunk = chunk->next;
	}
	s = s + "\nResponse: " + getResContents();
	return s;
}

size_t CCurlImpl::writeCallBack(void *contents, size_t size, size_t nmemb, void *buf)
{
	size_t realsize = size * nmemb;
	ResBuffer *resbuf = (ResBuffer *)buf;
	char *ptr;

	if (resbuf->buf) {
		ptr = (char*)realloc(resbuf->buf, resbuf->size + realsize + 1);
		if (!ptr) {
			return 0;
		}
	}
	else {
		resbuf->size = 0;
		ptr = (char*)malloc(realsize + 1);
	}

	memcpy(ptr + resbuf->size, contents, realsize);
	resbuf->buf = ptr;
	resbuf->size += realsize;
	resbuf->buf[resbuf->size] = 0;

	return realsize;
}
