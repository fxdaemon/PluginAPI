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
#include "SessionStatusListener.h"

CSessionStatusListener::CSessionStatusListener(IO2GSession* session, CLoginDataProvider* loginDataProvider, IPluginProxy *pluginProxy)
{
	m_pSession = session;
	m_pLoginDataProvider = loginDataProvider;
	m_pPluginProxy = pluginProxy;

	m_hSyncEvent = nsapi::CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hStopEvent = nsapi::CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hConnectedEvent = nsapi::CreateEvent(NULL, FALSE, FALSE, NULL);
}

CSessionStatusListener::~CSessionStatusListener()
{
	nsapi::CloseHandle(m_hSyncEvent);
	nsapi::CloseHandle(m_hStopEvent);
	nsapi::CloseHandle(m_hConnectedEvent);
	delete m_pLoginDataProvider;
}

void CSessionStatusListener::onSessionStatusChanged(O2GSessionStatus status)
{
	HANDLE hSyncEvent = m_hSyncEvent;

	switch (status) {
	case IO2GSessionStatus::Connecting:
		m_pPluginProxy->onMessage(MSG_INFO, "Status: Connecting");
		break;
	case IO2GSessionStatus::Connected:
		m_pPluginProxy->onMessage(MSG_INFO, "Status: Connected");
		hSyncEvent = m_hConnectedEvent;
		break;
	case IO2GSessionStatus::Disconnecting:
		m_pPluginProxy->onMessage(MSG_INFO, "Status: Disconnecting");
		break;
	case IO2GSessionStatus::Disconnected:
		m_pPluginProxy->onMessage(MSG_INFO, "Status: Disconnected");
		m_pPluginProxy->onDisconnected();
		hSyncEvent = m_hStopEvent;
		break;
	case IO2GSessionStatus::Reconnecting:
		m_pPluginProxy->onMessage(MSG_INFO, "Status: Reconnecting");
		break;
	case IO2GSessionStatus::PriceSessionReconnecting:
		m_pPluginProxy->onMessage(MSG_INFO, "Status: Price Session Reconnecting");
		break;
	case IO2GSessionStatus::SessionLost:
		m_pPluginProxy->onMessage(MSG_INFO, "Status: Session Lost");
		m_pPluginProxy->onDisconnected();
		hSyncEvent = m_hStopEvent;
		break;
	case IO2GSessionStatus::TradingSessionRequested:
		m_pPluginProxy->onMessage(MSG_INFO, "Status: TradingSessionRequested");
		{
			string sessionID = m_pLoginDataProvider->getSessionID();
			if (sessionID.empty()) {
				IO2GSessionDescriptorCollection *descriptors = m_pSession->getTradingSessionDescriptors();
				if (descriptors->size() > 0) {
					sessionID = descriptors->get(0)->getID();
				}
				descriptors->release();
			}
			m_pSession->setTradingSession(sessionID.c_str(), m_pLoginDataProvider->getPin().c_str());
		}
		break;
	}

	nsapi::SetEvent(hSyncEvent);
}

void CSessionStatusListener::onLoginFailed(const char* error)
{
	m_pPluginProxy->onMessage(MSG_ERROR, error);
	nsapi::SetEvent(m_hStopEvent);
}
