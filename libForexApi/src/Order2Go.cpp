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
#include "SimpleIni.h"
#include "Utils.h"
#include "Order2Go.h"

static const struct {
	const char* period;
	time_t timet;
} period2time[] = {
	{ "m1", 1 * 60 },
	{ "m5", 5 * 60 },
	{ "m15", 15 * 60 }, 
	{ "m30", 30 * 60 },
	{ "H1", 1 * 3600 },
	{ "H2", 2 * 3600 },
	{ "H3", 3 * 3600 },
	{ "H4", 4 * 3600 },
	{ "H6", 6 * 3600 },
	{ "H8", 8 * 3600 },
	{ "D1", 24 * 3600 },
	{ "W1", 7 * 24 * 3600 },
	{ 0, 0 }
};

static const int CandleMaxNumber = 240;
static const char* TimeFormat = "%Y-%m-%d %H:%M:%S";

static COrder2Go order2Go;

COrder2Go::COrder2Go()
{
	m_pPluginProxy = getPluginProxy();
	m_pPluginProxy->registerPlugin("Order2Go", this);
}

int COrder2Go::init(const char* iniFile)
{
	m_SimpleIni.SetUnicode();
	if (m_SimpleIni.LoadFile(iniFile) < 0) {
		m_pPluginProxy->onMessage(MSG_ERROR, "Config file open failed.");
		return RET_FAILED;
	}

	m_pSession = CO2GTransport::createSession();
	if (!m_pSession) {
		m_pPluginProxy->onMessage(MSG_ERROR, "Failed to create session.");
		return RET_FAILED;
	}

	m_pSession->useTableManager(::Yes, NULL);
	m_pTableListener = new CTableListener(m_pPluginProxy);

	m_pSessionStatusListener = new CSessionStatusListener(m_pSession,
		new CLoginDataProvider(getLoginInfo("SessionID"), getLoginInfo("Pin")), m_pPluginProxy);
	m_pSession->subscribeSessionStatus(m_pSessionStatusListener);

	m_pResponseListener = new CResponseListener();
	m_pSession->subscribeResponse(m_pResponseListener);

	return RET_SUCCESS;
}

int COrder2Go::login()
{
	string msg = "login... ";
	msg.append(" Host:").append(getLoginInfo("Host")).append(" UserName:").append(getLoginInfo("UserName"));
	m_pPluginProxy->onMessage(MSG_INFO, msg.c_str());

	m_pSession->login(
		getLoginInfo("UserName"), getLoginInfo("Password"),
		getLoginInfo("Host"), getLoginInfo("Connection"));

	HANDLE handles[2];
	handles[0] = m_pSessionStatusListener->getConnectedEvent();
	handles[1] = m_pSessionStatusListener->getStopEvent();
	DWORD dwRes = nsapi::WaitForMultipleObjects(2, handles, FALSE, INFINITE);
	if (dwRes == WAIT_OBJECT_0) {
		onConnected();
		return subscribeTableListener(m_pSession->getTableManager(), m_pTableListener);
	}
	return RET_FAILED;
}

int COrder2Go::close()
{
	int ret = RET_SUCCESS;

	if (m_pSession->getSessionStatus() == IO2GSessionStatus::Connected) {
		unsubscribeTableListener(m_pSession->getTableManager(), m_pTableListener);
	}
	m_pTableListener->release();
		
	m_pSession->unsubscribeResponse(m_pResponseListener);
	m_pResponseListener->release();
	if (m_pSession->getSessionStatus() == IO2GSessionStatus::Connected) {
		ret = logout();
	}
		
	m_pSession->unsubscribeSessionStatus(m_pSessionStatusListener);
	m_pSessionStatusListener->release();
		
	m_pSession->release();

	return ret;
}

time_t COrder2Go::getServerTime()
{
	return CTableListener::date2Time(m_pSession->getServerTime());
}

int COrder2Go::getAccount(const char* accountID, TblAccount** tblAccount)
{
	*tblAccount = NULL;
	IO2GTableManager *tableManager = m_pSession->getTableManager();
	IO2GAccountsTable *accountsTable = (IO2GAccountsTable*)tableManager->getTable(::Accounts);

	IO2GAccountTableRow *accountRow = NULL;
	IO2GTableIterator tableIterator;
	while (accountsTable->getNextRow(tableIterator, accountRow)) {
		if (accountRow->getMarginCallFlag()[0] == 'N' &&
			(strlen(accountID) == 0 || strcmp(accountRow->getAccountID(), accountID) == 0)) {
			*tblAccount = CTableListener::makTblAccount(accountRow);
			accountRow->release();
			break;
		}
		accountRow->release();
	}
	accountsTable->release();

	return *tblAccount ? 1 : 0;
}

int COrder2Go::getPrice(const char* symbols[], TblPrice** pTblPrice[])
{
	IO2GTableManager *tableManager = m_pSession->getTableManager();
	IO2GOffersTable *offersTable = (IO2GOffersTable*)tableManager->getTable(::Offers);

	vector<TblPrice*> tblPriceList;
	IO2GOfferTableRow *offerRow = NULL;
	IO2GTableIterator tableIterator;
	while (offersTable->getNextRow(tableIterator, offerRow)) {
		for (int i = 0; symbols[i]; i++) {
			if (strcmp(offerRow->getInstrument(), symbols[i]) == 0) {
				tblPriceList.push_back(CTableListener::makTblPrice(offerRow));
				break;
			}
		}
		offerRow->release();
	}
	offersTable->release();

	if (tblPriceList.size() > 0) {
		*pTblPrice = new TblPrice*[tblPriceList.size()];
		std::copy(tblPriceList.begin(), tblPriceList.end(), *pTblPrice);
	}

	return tblPriceList.size();
}

int COrder2Go::getOpenedTrades(TblTrade** pTblTrade[])
{
	IO2GTableManager *tableManager = m_pSession->getTableManager();
	IO2GTradesTable *tradesTable = (IO2GTradesTable*)tableManager->getTable(::Trades);

	vector<TblTrade*> tblTradeList;
	IO2GTradeTableRow *tradeRow = NULL;
	IO2GTableIterator tableIterator;
	while (tradesTable->getNextRow(tableIterator, tradeRow)) {
		tblTradeList.push_back(CTableListener::makOpenTblTrade(tradeRow));
		tradeRow->release();
	}
	tradesTable->release();

	if (tblTradeList.size() > 0) {
		*pTblTrade = new TblTrade*[tblTradeList.size()];
		std::copy(tblTradeList.begin(), tblTradeList.end(), *pTblTrade);
	}

	return tblTradeList.size();
}

int COrder2Go::getClosedTrades(TblTrade** pTblTrade[])
{
	IO2GTableManager *tableManager = m_pSession->getTableManager();
	IO2GClosedTradesTable *tradesTable = (IO2GClosedTradesTable*)tableManager->getTable(::ClosedTrades);

	vector<TblTrade*> tblTradeList;
	IO2GClosedTradeTableRow *tradeRow = NULL;
	IO2GTableIterator tableIterator;
	while (tradesTable->getNextRow(tableIterator, tradeRow)) {
		tblTradeList.push_back(CTableListener::makClosedTblTrade(tradeRow));
		tradeRow->release();
	}
	tradesTable->release();

	if (tblTradeList.size() > 0) {
		*pTblTrade = new TblTrade*[tblTradeList.size()];
		std::copy(tblTradeList.begin(), tblTradeList.end(), *pTblTrade);
	}

	return tblTradeList.size();
}

int COrder2Go::getHistoricalData(const char* symbol, const char* period, time_t start, time_t end, bool maxRange, TblCandle** pTblCandle[])
{
	// UTC (Sunday 19:00 - Friday 21:00)
	int marketOpenWday = atoi(getMarketInfo("OpenWday", "0"));
	int marketOpenHour = atoi(getMarketInfo("OpenHour", "19"));
	int marketCloseWday = atoi(getMarketInfo("CloseWday", "5"));
	int marketCloseHour = atoi(getMarketInfo("CloseHour", "21"));
	time_t interval = getTimetByPeriod(period);
	
	vector<TblCandle*> outCandleList;
	time_t cur = start - interval;
	time_t addLastTime = start;

	while (cur <= end) {
		vector<TblCandle*> tmpCandleList;
		getHistoricalData(symbol, period, cur, CandleMaxNumber, tmpCandleList);
		if (!tmpCandleList.empty()) {
			if (!outCandleList.empty()) {
				addLastTime = outCandleList.back()->StartDate;
			}

			bool isAddNew = false;
			vector<TblCandle*>::iterator vpos = tmpCandleList.begin();
			while (vpos != tmpCandleList.end()) {
				if ((*vpos)->StartDate > addLastTime && (*vpos)->StartDate <= end - interval) {
					outCandleList.push_back(*vpos);
					isAddNew = true;
				}
				else {
					delete *vpos;
				}
				++vpos;
			}

			if (isAddNew) {
				cur = outCandleList.back()->StartDate - interval;
				continue;
			}
		}
		cur = CUtils::overDayoff(cur, marketOpenWday, marketOpenHour, marketCloseWday, marketCloseHour);
	}
	
	if (outCandleList.size() > 0) {
		*pTblCandle = new TblCandle*[outCandleList.size()];
		std::copy(outCandleList.begin(), outCandleList.end(), *pTblCandle);
	}

	return outCandleList.size();
}

int COrder2Go::openMarketOrder(TblOrder* tblOrder)
{
	string orderID;
	IO2GRequestFactory *requestFactory = m_pSession->getRequestFactory();
	IO2GValueMap *valuemap = requestFactory->createValueMap();
	valuemap->setString(Command, O2G2::Commands::CreateOrder);
	valuemap->setString(OrderType, O2G2::Orders::TrueMarketOpen);
	valuemap->setString(AccountID, tblOrder->AccountID);
	valuemap->setString(OfferID, tblOrder->OfferID);
	valuemap->setString(BuySell, tblOrder->BS);
	valuemap->setInt(Amount, (int)tblOrder->Amount);
	valuemap->setDouble(RateStop, tblOrder->Stop);
	valuemap->setDouble(RateLimit, tblOrder->Limit);
	valuemap->setInt(TrailStep, 0);
	valuemap->setString(TimeInForce, O2G2::TIF::IOC);

	IO2GRequest *request = requestFactory->createOrderRequest(valuemap);
	valuemap->release();
	if (!request) {
		requestFactory->release();
		return RET_FAILED;
	}

	m_pResponseListener->setRequestID(request->getRequestID());
	m_pSession->sendRequest(request);
	request->release();
	requestFactory->release();

	nsapi::WaitForSingleObject(m_pResponseListener->getReponseEvent(), INFINITE);
	CResponse* response = m_pResponseListener->popResponse();
	if (response) {
		if (response->getResponseStatus() == CResponse::COMPLETED) {
			IO2GResponseReaderFactory *factory = m_pSession->getResponseReaderFactory();
			if (factory) {
				IO2GOrderResponseReader *responseReader  = factory->createOrderResponseReader(response->getIResponse());
				if (responseReader) {
					orderID = responseReader->getOrderID();
					responseReader->release();
				}
				factory->release();
			}
		} else {
			m_pPluginProxy->onMessage(MSG_ERROR, response->getError().c_str());
		}
		delete response;
	}
	
	if (orderID.empty()) {
		return RET_FAILED;
	}
	strcpy(tblOrder->OrderID, orderID.c_str());
	return RET_SUCCESS;
}

int COrder2Go::changeStopLoss(TblTrade* tblTrade)
{
	int ret = RET_SUCCESS;
	IO2GRequestFactory *requestFactory = m_pSession->getRequestFactory();
	IO2GValueMap *valuemap = requestFactory->createValueMap();
	string stopOrderID = getStopOrderID(tblTrade->TradeID);
	if (stopOrderID.empty()) {
		valuemap->setString(Command, O2G2::Commands::CreateOrder);
		valuemap->setString(OrderType, O2G2::Orders::Stop);
		valuemap->setString(AccountID, tblTrade->AccountID);
		valuemap->setString(OfferID, tblTrade->OfferID);
		valuemap->setString(TradeID, tblTrade->TradeID);
		valuemap->setString(BuySell, strcmp(tblTrade->BS, "B") == 0 ? O2G2::Sell : O2G2::Buy);
		valuemap->setInt(Amount, (int)tblTrade->Amount);
		valuemap->setDouble(Rate, tblTrade->Stop);
	}
	else {
		valuemap->setString(Command, O2G2::Commands::EditOrder);
		valuemap->setString(OrderID, stopOrderID.c_str());
		valuemap->setString(AccountID, tblTrade->AccountID);
		valuemap->setDouble(Rate, tblTrade->Stop);
	}
	IO2GRequest *request = requestFactory->createOrderRequest(valuemap);
	if (request) {
		m_pResponseListener->setRequestID(request->getRequestID());
		m_pSession->sendRequest(request);
		request->release();
		nsapi::WaitForSingleObject(m_pResponseListener->getReponseEvent(), INFINITE);
		CResponse* response = m_pResponseListener->popResponse();
		if (response) {
			if (response->getResponseStatus() == CResponse::FAILED) {
				m_pPluginProxy->onMessage(MSG_ERROR, response->getError().c_str());
				ret = RET_FAILED;
			}
			delete response;
		}
	}
	valuemap->release();
	requestFactory->release();
	return ret;
}

int COrder2Go::changeTakeProfit(TblTrade* tblTrade)
{
	int ret = RET_SUCCESS;
	IO2GRequestFactory *requestFactory = m_pSession->getRequestFactory();
	IO2GValueMap *valuemap = requestFactory->createValueMap();
	string limitOrderID = getLimitOrderID(tblTrade->TradeID);
	if (limitOrderID.empty()) {
		valuemap->setString(Command, O2G2::Commands::CreateOrder);
		valuemap->setString(OrderType, O2G2::Orders::Limit);
		valuemap->setString(AccountID, tblTrade->AccountID);
		valuemap->setString(OfferID, tblTrade->OfferID);
		valuemap->setString(TradeID, tblTrade->TradeID);
		valuemap->setString(BuySell, strcmp(tblTrade->BS, "B") == 0 ? O2G2::Sell : O2G2::Buy);
		valuemap->setInt(Amount, (int)tblTrade->Amount);
		valuemap->setDouble(Rate, tblTrade->Limit);
	}
	else {
		valuemap->setString(Command, O2G2::Commands::EditOrder);
		valuemap->setString(OrderID, limitOrderID.c_str());
		valuemap->setString(AccountID, tblTrade->AccountID);
		valuemap->setDouble(Rate, tblTrade->Limit);
	}
	IO2GRequest *request = requestFactory->createOrderRequest(valuemap);
	if (request) {
		m_pResponseListener->setRequestID(request->getRequestID());
		m_pSession->sendRequest(request);
		request->release();
		nsapi::WaitForSingleObject(m_pResponseListener->getReponseEvent(), INFINITE);
		CResponse* response = m_pResponseListener->popResponse();
		if (response) {
			if (response->getResponseStatus() == CResponse::FAILED) {
				m_pPluginProxy->onMessage(MSG_ERROR, response->getError().c_str());
				ret = RET_FAILED;
			}
			delete response;
		}
	}
	valuemap->release();
	requestFactory->release();
	return ret;
}

int COrder2Go::closeTrade(TblTrade* tblTrade)
{
	int ret = RET_SUCCESS;
	IO2GRequestFactory *requestFactory = m_pSession->getRequestFactory();
	IO2GValueMap *valuemap = requestFactory->createValueMap();
	valuemap->setString(Command, O2G2::Commands::CreateOrder);
	valuemap->setString(OrderType, O2G2::Orders::TrueMarketClose);
	valuemap->setString(AccountID, tblTrade->AccountID);
	valuemap->setString(OfferID, tblTrade->OfferID);
	valuemap->setString(TradeID, tblTrade->TradeID);
	valuemap->setString(BuySell, strcmp(tblTrade->BS, "B") == 0 ? O2G2::Sell : O2G2::Buy);
	valuemap->setInt(Amount, tblTrade->Amount);
	IO2GRequest *request = requestFactory->createOrderRequest(valuemap);
	if (request) {
		m_pResponseListener->setRequestID(request->getRequestID());
		m_pSession->sendRequest(request);
		request->release();
		nsapi::WaitForSingleObject(m_pResponseListener->getReponseEvent(), INFINITE);
		CResponse* response = m_pResponseListener->popResponse();
		if (response) {
			if (response->getResponseStatus() == CResponse::FAILED) {
				m_pPluginProxy->onMessage(MSG_ERROR, response->getError().c_str());
				ret = RET_FAILED;
			}
			delete response;
		}
	}

	valuemap->release();
	requestFactory->release();
	return ret;
}

int COrder2Go::logout()
{
	m_pSession->logout();
	if (nsapi::WaitForSingleObject(m_pSessionStatusListener->getStopEvent(), INFINITE) == WAIT_OBJECT_0) {
		return RET_SUCCESS;
	}
	return RET_FAILED;
}

void COrder2Go::onConnected()
{
	logTimeFrames();

	IO2GLoginRules *loginRules = m_pSession->getLoginRules();
	if (loginRules) {
		IO2GResponse *systemPropertiesResponse = loginRules->getSystemPropertiesResponse();
		onSystemPropertiesReceived(systemPropertiesResponse);
		systemPropertiesResponse->release();
		loginRules->release();
		printSettings();
	}
}

void COrder2Go::logTimeFrames()
{
	IO2GRequestFactory *factory = m_pSession->getRequestFactory();
	IO2GTimeframeCollection *timeFrames = factory->getTimeFrameCollection();
	string outSrt = "[TimeFrames]";
	char szBuffer[256];
	for (int i = 0; i < timeFrames->size(); i++) {
		IO2GTimeframe *timeFrame = timeFrames->get(i);
		sprintf_s(szBuffer, sizeof(szBuffer), "\nTime frame id=%s unit=%d size=%d", timeFrame->getID(), timeFrame->getUnit(), timeFrame->getSize());
		outSrt = outSrt + szBuffer;
		timeFrame->release();
	}
	m_pPluginProxy->onMessage(MSG_DEBUG, outSrt.c_str());
	timeFrames->release();
	factory->release();
}

void COrder2Go::printSettings()
{
	O2G2Ptr<IO2GLoginRules> loginRules = m_pSession->getLoginRules();
	O2G2Ptr<IO2GResponse> accountsResponse = loginRules->getTableRefreshResponse(Accounts);
	if (!accountsResponse) {
		return;
	}
	O2G2Ptr<IO2GResponse> offersResponse = loginRules->getTableRefreshResponse(Offers);
	if (!offersResponse) {
		return;
	}

	O2G2Ptr<IO2GTradingSettingsProvider> tradingSettingsProvider = loginRules->getTradingSettingsProvider();
	O2G2Ptr<IO2GResponseReaderFactory> factory = m_pSession->getResponseReaderFactory();
	if (!factory) {
		return;
	}
	O2G2Ptr<IO2GAccountsTableResponseReader> accountsReader = factory->createAccountsTableReader(accountsResponse);
	if (!accountsReader) {
		return;
	}
	O2G2Ptr<IO2GOffersTableResponseReader> instrumentsReader = factory->createOffersTableReader(offersResponse);
	if (!instrumentsReader) {
		return;
	}
	O2G2Ptr<IO2GAccountRow> account = accountsReader->getRow(0);
	if (!account) {
		return;
	}

	string settingStr = "Settings:";
	for (int i = 0; i < instrumentsReader->size(); ++i) {
		O2G2Ptr<IO2GOfferRow> instrumentRow = instrumentsReader->getRow(i);
		const char* instrument = instrumentRow->getInstrument();
		int condDistStopForTrade = tradingSettingsProvider->getCondDistStopForTrade(instrument);
		int condDistLimitForTrade = tradingSettingsProvider->getCondDistLimitForTrade(instrument);
		int condDistEntryStop = tradingSettingsProvider->getCondDistEntryStop(instrument);
		int condDistEntryLimit = tradingSettingsProvider->getCondDistEntryLimit(instrument);
		int minQuantity = tradingSettingsProvider->getMinQuantity(instrument, account);
		int maxQuantity = tradingSettingsProvider->getMaxQuantity(instrument, account);
		int baseUnitSize = tradingSettingsProvider->getBaseUnitSize(instrument, account);
		O2GMarketStatus marketStatus = tradingSettingsProvider->getMarketStatus(instrument);
		int minTrailingStep = tradingSettingsProvider->getMinTrailingStep();
		int maxTrailingStep = tradingSettingsProvider->getMaxTrailingStep();
		double mmr = tradingSettingsProvider->getMMR(instrument, account);
		double mmr2, emr, lmr;
		bool threeLevelMargin = tradingSettingsProvider->getMargins(instrument, account, mmr2, emr, lmr);

		char buffer[256];
		const char* marketStatusStr = "unknown";
		switch (marketStatus) {
		case MarketStatusOpen:
			marketStatusStr = "Market Open";
			break;
		case MarketStatusClosed:
			marketStatusStr = "Market Close";
			break;
		}

		sprintf_s(buffer, sizeof(buffer), "\n  [%s (%s)] Cond.Dist: ST=%d; LT=%d", instrument, marketStatusStr, condDistStopForTrade, condDistLimitForTrade);
		settingStr += buffer;
		sprintf_s(buffer, sizeof(buffer), "\n            Cond.Dist entry stop=%d; entry limit=%d", condDistEntryStop, condDistEntryLimit);
		settingStr += buffer;
		sprintf_s(buffer, sizeof(buffer), "\n            Quantity: Min=%d; Max=%d. Base unit size=%d; MMR=%f", minQuantity, maxQuantity, baseUnitSize, mmr);
		settingStr += buffer;
		if (threeLevelMargin) {
			sprintf_s(buffer, sizeof(buffer), "\n            Three level margin: MMR=%f; EMR=%f; LMR=%f", mmr2, emr, lmr);
		} else {
			sprintf_s(buffer, sizeof(buffer), "\n            Single level margin: MMR=%f; EMR=%f; LMR=%f", mmr2, emr, lmr);
		}
		settingStr += buffer;
		sprintf_s(buffer, sizeof(buffer), "\n            Trailing step: %d-%d", minTrailingStep, maxTrailingStep);
		settingStr += buffer;
	}
	m_pPluginProxy->onMessage(MSG_DEBUG, settingStr.c_str());
}

void COrder2Go::onSystemPropertiesReceived(IO2GResponse *response)
{
	IO2GResponseReaderFactory *factory = m_pSession->getResponseReaderFactory();
	if (factory == NULL) {
		return;
	}
	IO2GSystemPropertiesReader *systemResponseReader = factory->createSystemPropertiesReader(response);
	if (systemResponseReader == NULL) {
		factory->release();
		return;
	}

	string s = "[System Properties]";
	for (int i = 0; i < systemResponseReader->size(); i++) {
		const char *value;
		const char *sysProperty = systemResponseReader->getProperty(i, value);
		s = s + "\n" + sysProperty + " = " + value;
	}
	m_pPluginProxy->onMessage(MSG_DEBUG, s.c_str());

	systemResponseReader->release();
	factory->release();
}

int COrder2Go::subscribeTableListener(IO2GTableManager *manager, IO2GTableListener *listener)
{
	while (manager->getStatus() != TablesLoaded && manager->getStatus() != TablesLoadFailed) {
		nsapi::Sleep(50);
	}
	if (manager->getStatus() == TablesLoaded) {
		m_bSubscribed = true;
		O2G2Ptr<IO2GOffersTable> offersTable = (IO2GOffersTable*)manager->getTable(::Offers);
		O2G2Ptr<IO2GAccountsTable> accountsTable = (IO2GAccountsTable*)manager->getTable(::Accounts);
		O2G2Ptr<IO2GOrdersTable> ordersTable = (IO2GOrdersTable *)manager->getTable(::Orders);
		O2G2Ptr<IO2GTradesTable> tradesTable = (IO2GTradesTable*)manager->getTable(::Trades);
		O2G2Ptr<IO2GClosedTradesTable> closeTradesTable = (IO2GClosedTradesTable*)manager->getTable(::ClosedTrades);
		offersTable->subscribeUpdate(Update, listener);
		accountsTable->subscribeUpdate(Update, listener);
		ordersTable->subscribeUpdate(Insert, listener);
		ordersTable->subscribeUpdate(Update, listener);
		ordersTable->subscribeUpdate(Delete, listener);
		tradesTable->subscribeUpdate(Insert, listener);
		tradesTable->subscribeUpdate(Update, listener);
		tradesTable->subscribeUpdate(Delete, listener);
		closeTradesTable->subscribeUpdate(Insert, listener);
		return RET_SUCCESS;
	}
	return RET_FAILED;
}

void COrder2Go::unsubscribeTableListener(IO2GTableManager *manager, IO2GTableListener *listener)
{
	if (m_bSubscribed) {
		O2G2Ptr<IO2GOffersTable> offersTable = (IO2GOffersTable*)manager->getTable(::Offers);
		O2G2Ptr<IO2GAccountsTable> accountsTable = (IO2GAccountsTable*)manager->getTable(::Accounts);
		O2G2Ptr<IO2GOrdersTable> ordersTable = (IO2GOrdersTable *)manager->getTable(::Orders);
		O2G2Ptr<IO2GTradesTable> tradesTable = (IO2GTradesTable*)manager->getTable(::Trades);
		O2G2Ptr<IO2GClosedTradesTable> closeTradesTable = (IO2GClosedTradesTable*)manager->getTable(::ClosedTrades);
		offersTable->unsubscribeUpdate(Update, listener);
		accountsTable->unsubscribeUpdate(Update, listener);
		ordersTable->unsubscribeUpdate(Insert, listener);
		ordersTable->unsubscribeUpdate(Update, listener);
		ordersTable->unsubscribeUpdate(Delete, listener);
		tradesTable->unsubscribeUpdate(Insert, listener);
		tradesTable->unsubscribeUpdate(Update, listener);
		tradesTable->unsubscribeUpdate(Delete, listener);
		closeTradesTable->unsubscribeUpdate(Insert, listener);
	}
}

int COrder2Go::getHistoricalData(const char* symbol, const char* period, time_t start, int maxNumber, vector<TblCandle*>& tblCandleList)
{
	time_t end = start + getTimetByPeriod(period) * maxNumber;
	int ret = getHistoricalData(symbol, period, start, end, maxNumber, tblCandleList);
	if (ret > 0) {
		char buf[256];
		tm tmStart = CUtils::getUTCCal(start);
		tm tmEnd = CUtils::getUTCCal(end);
		tm tmOutStart = CUtils::getUTCCal(tblCandleList.front()->StartDate);
		tm tmOutEnd = CUtils::getUTCCal(tblCandleList.back()->StartDate);
		sprintf(buf, "[GetHistoricalData] Count:%d StartDate:%s EndDate:%s OutStartDate:%s OutEndDate:%s",
			ret,
			CUtils::strOfTime(&tmStart, TimeFormat).c_str(),
			CUtils::strOfTime(&tmEnd, TimeFormat).c_str(),
			CUtils::strOfTime(&tmOutStart, TimeFormat).c_str(),
			CUtils::strOfTime(&tmOutEnd, TimeFormat).c_str());
		m_pPluginProxy->onMessage(MSG_DEBUG, buf);
	}
	return ret;
}

int COrder2Go::getHistoricalData(const char* symbol, const char* period, time_t start, time_t end, int maxNumber, vector<TblCandle*>& tblCandleList)
{
	DATE dtStart = CTableListener::time2Date(CUtils::getUTCCal(start));
	DATE dtEnd = CTableListener::time2Date(CUtils::getUTCCal(end));
	return getHistoricalData(symbol, period, dtStart, dtEnd, maxNumber, tblCandleList);
}

int COrder2Go::getHistoricalData(const char* symbol, const char* period, double start, double end, int maxNumber, vector<TblCandle*>& tblCandleList)
{
	IO2GRequestFactory *requestFactory = m_pSession->getRequestFactory();
	IO2GTimeframeCollection * timeFrames = requestFactory->getTimeFrameCollection();
	IO2GTimeframe * timeFrame = timeFrames->get(period);
	IO2GRequest * request = requestFactory->createMarketDataSnapshotRequestInstrument(symbol, timeFrame, maxNumber);
	requestFactory->fillMarketDataSnapshotRequestTime(request, start, end, true);
	m_pResponseListener->setRequestID(request->getRequestID());
	m_pSession->sendRequest(request);
	request->release();
	requestFactory->release();

	int ret = 0;
	nsapi::WaitForSingleObject(m_pResponseListener->getReponseEvent(), INFINITE);
	CResponse* response = m_pResponseListener->popResponse();
	if (!response) {
		return ret;
	}

	if (response->getResponseStatus() == CResponse::COMPLETED) {
		IO2GResponseReaderFactory *factory = m_pSession->getResponseReaderFactory();
		if (factory) {
			IO2GMarketDataSnapshotResponseReader *reader = factory->createMarketDataSnapshotReader(response->getIResponse());
			if (reader) {
				ret = candleOfReader(symbol, period, reader, tblCandleList);
				reader->release();
			}
			factory->release();
		}
	}
	else {
		const std::string& error = response->getError();
		if (error.find("unsupported scope") == std::string::npos) {
			ret = RET_FAILED;
			m_pPluginProxy->onMessage(MSG_ERROR, error.c_str());
		}
	}

	delete response;
	return ret;
}

int COrder2Go::candleOfReader(const char* symbol, const char* period, IO2GMarketDataSnapshotResponseReader* reader, vector<TblCandle*>& tblCandleList)
{
	if (!reader->isBar()) {
		return 0;
	}
	
	for (int i = 0; i < reader->size(); i++) {
		TblCandle* tblCandle = new TblCandle();
		strcpy(tblCandle->Symbol, symbol);
		tblCandle->StartDate = CTableListener::date2Time(reader->getDate(i));
		strcpy(tblCandle->Period, period);
		tblCandle->AskClose = reader->getAskClose(i);
		tblCandle->AskHigh = reader->getAskHigh(i);
		tblCandle->AskLow = reader->getAskLow(i);
		tblCandle->AskOpen = reader->getAskOpen(i);
		tblCandle->BidClose = reader->getBidClose(i);
		tblCandle->BidHigh = reader->getBidHigh(i);
		tblCandle->BidLow = reader->getBidLow(i);
		tblCandle->BidOpen = reader->getBidOpen(i);
		tblCandleList.push_back(tblCandle);
	}

	return tblCandleList.size();
}

string COrder2Go::getOrderID(string tradeID, string orderType)
{
	string orderID;
	IO2GTableManager *tableManager = m_pSession->getTableManager();
	while (tableManager->getStatus() != TablesLoaded && tableManager->getStatus() != TablesLoadFailed) {
		nsapi::Sleep(50);
	}
	if (tableManager->getStatus() == TablesLoaded) {
		IO2GOrdersTable *ordersTable = (IO2GOrdersTable*)tableManager->getTable(::Orders);
		IO2GOrderTableRow *orderRow = NULL;
		IO2GTableIterator tableIterator;
		while (ordersTable->getNextRow(tableIterator, orderRow)) {
			if (orderRow->getTradeID() == tradeID && orderRow->getType() == orderType) {
				orderID = orderRow->getOrderID();
				orderRow->release();
				break;
			}
			orderRow->release();
		}
		ordersTable->release();
	}
	return orderID;
}

string COrder2Go::getLimitOrderID(string tradeID)
{
	return getOrderID(tradeID, "L");
}

string COrder2Go::getStopOrderID(string tradeID)
{
	return getOrderID(tradeID, "S");
}

const char* COrder2Go::getLoginInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("Login", key, defval);
}

const char* COrder2Go::getMarketInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("Market", key, defval);
}

time_t COrder2Go::getTimetByPeriod(const char* period)
{
	for (int i = 0; period2time[i].period; i++) {
		if (strcmp(period, period2time[i].period) == 0) {
			return period2time[i].timet;
		}
	}
	return 0;
}
