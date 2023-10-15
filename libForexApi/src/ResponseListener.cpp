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
#include "ResponseListener.h"

CResponseListener::CResponseListener()
{
	m_hResponse = nsapi::CreateEvent(NULL, FALSE, FALSE, NULL);
}

CResponseListener::~CResponseListener()
{
	nsapi::CloseHandle(m_hResponse);
	clearResponseQueue();
}

void CResponseListener::setRequestID(const char* requestID)
{
	m_sRequestID = requestID;
}

void CResponseListener::onRequestCompleted(const char* requestID, IO2GResponse* response)
{
	if (requestID && m_sRequestID == requestID) {
		response->addRef();
		pushResponse(new CResponse(CResponse::COMPLETED, response, requestID, ""));
		nsapi::SetEvent(m_hResponse);
	}
}

void CResponseListener::onRequestFailed(const char* requestID , const char* error)
{
	if (requestID && m_sRequestID == requestID) {
		pushResponse(new CResponse(CResponse::FAILED, NULL, requestID, error));
		nsapi::SetEvent(m_hResponse);
	}
}

void CResponseListener::onTablesUpdates(IO2GResponse* data)
{
}

void CResponseListener::pushResponse(CResponse* response)
{
	CCriticalSection::Lock l(m_csQueue);
	m_qeResponses.push(response);
}

CResponse* CResponseListener::popResponse()
{
	CResponse* response = NULL;
	CCriticalSection::Lock l(m_csQueue);
	if (!m_qeResponses.empty()) {
		response = m_qeResponses.front();
		m_qeResponses.pop();
	}
	return response;
}

void CResponseListener::clearResponseQueue()
{
	CCriticalSection::Lock l(m_csQueue);
	while (!m_qeResponses.empty()) {
		CResponse *p = m_qeResponses.front();
		m_qeResponses.pop();
		if (p) {
			delete p;
		}
	}
}

