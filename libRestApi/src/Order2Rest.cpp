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
#include "picojson.h"
#include "Thread.h"
#include "Utils.h"
#include "CurlImpl.h"
#include "Order2Rest.h"

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

static const int CandleMaxNumber = 2000;
static const char* TimeFormat = "%Y-%m-%d %H:%M:%S";

static COrder2Rest order2Rest;

COrder2Rest::COrder2Rest()
{
	m_pPluginProxy = getPluginProxy();
	m_pPluginProxy->registerPlugin("Order2Rest", this);
	
	m_pCurlMulti = NULL;
	m_hExitEvent = nsapi::CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hOverEvent = nsapi::CreateEvent(NULL, FALSE, FALSE, NULL);
	ThreadFunAttr threadFunAttr = { tradeEventsProcess, this };
	m_pTradeEventsProcessThread = new CThread(threadFunAttr);
	m_tmStdTime.store(0);
}

COrder2Rest::~COrder2Rest()
{
	nsapi::CloseHandle(m_hExitEvent);
	nsapi::CloseHandle(m_hOverEvent);
	delete m_pTradeEventsProcessThread;
}

int COrder2Rest::init(const char* iniFile)
{
	m_SimpleIni.SetUnicode();
	if (m_SimpleIni.LoadFile(iniFile) < 0) {
		m_pPluginProxy->onMessage(MSG_ERROR, "Config file open failed.");
		return RET_FAILED;
	}

	curl_global_init(CURL_GLOBAL_ALL);
	if (strcmp(getBaseInfo("parallel"), "true") == 0) {
		m_pCurlMulti = curl_multi_init();
		if (m_pCurlMulti) {
			m_pPluginProxy->onMessage(MSG_INFO, "curl_multi_init succeeded.");
		}
		else {
			m_pPluginProxy->onMessage(MSG_ERROR, "Can't init multi curl.");
			return RET_FAILED;
		}
	}

	m_mapPathParams.insert(pair<string, string>("$account_id", getBaseInfo("AccountID")));
	return initCurl();
}

int COrder2Rest::login()
{
	m_pTradeEventsProcessThread->_start();
	return RET_SUCCESS;
}

int COrder2Rest::close()
{
	nsapi::SetEvent(m_hExitEvent);
	nsapi::WaitForSingleObject(m_hOverEvent, INFINITE);

	if (m_pCurlMulti) {
		curl_multi_cleanup(m_pCurlMulti);
	}
	for (int i = 0; i < sizeof(m_CurlList) / sizeof(m_CurlList[0]); i++) {
		if (m_CurlList[i]) {
			delete m_CurlList[i];
		}
	}
	curl_global_cleanup();
	return RET_SUCCESS;
}

time_t COrder2Rest::getServerTime()
{
	return m_tmStdTime.load();
}

int COrder2Rest::getAccount(const char* accountID, TblAccount** tblAccount)
{
	CCurlImpl* curlObj = m_CurlList[CURL_GET_ACCOUNT];
	CURLcode ret = curlObj->doEasyPerform();
	if (ret != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, curl_easy_strerror(ret));
		return RET_FAILED;
	}

	picojson::value json;
	picojson::object& obj = parseJsonObject(curlObj, json);
	if (obj.empty()) {
		return 0;
	}
	
	*tblAccount = newTblAccount(obj, curlObj);
	curlObj->setRefreshInterval(atol(getAccountInfo("Refresh")));
	return 1;
}

int COrder2Rest::getPrice(const char* symbol[], TblPrice** pTblPrice[])
{
	CCurlImpl* curlObj = m_CurlList[CURL_GET_PRICE];

	vector<ReqParam> params;
	params.push_back(ReqParam{"$symbols", transfSymbols(symbol)});
	curlObj->setEasyPerform(&params);

	CURLcode ret = curlObj->doEasyPerform();
	if (ret != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, curl_easy_strerror(ret));
		return RET_FAILED;
	}

	picojson::value json;
	picojson::array& list = parseJsonArray(curlObj, json);
	if (list.empty()) {
		return 0;
	}

	vector<TblPrice*> tblPriceList;
	for (picojson::array::iterator it = list.begin(); it != list.end(); it++) {
		picojson::object& o = it->get<picojson::object>();
		tblPriceList.push_back(newTblPrice(o, curlObj));
	}

	if (tblPriceList.size() > 0) {
		*pTblPrice = new TblPrice*[tblPriceList.size()];
		std::copy(tblPriceList.begin(), tblPriceList.end(), *pTblPrice);
	}

	m_tmStdTime.store(reqServerTime());
	curlObj->setRefreshInterval(atol(getPriceInfo("Refresh")));
	return tblPriceList.size();
}

int COrder2Rest::getOpenedTrades(TblTrade** pTblTrade[])
{
	CCurlImpl* curlObj = m_CurlList[CURL_GET_OPENTRADES];
	CURLcode ret = curlObj->doEasyPerform();
	if (ret != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, curl_easy_strerror(ret));
		return RET_FAILED;
	}

	picojson::value json;
	picojson::array& list = parseJsonArray(curlObj, json);
	if (list.empty()) {
		curlObj->setRefreshInterval(atol(GetOpenedTradesInfo("Refresh")));
		return 0;
	}
	
	vector<TblTrade*> tblTradeList;
	for (picojson::array::iterator it = list.begin(); it != list.end(); it++) {
		picojson::object& o = it->get<picojson::object>();
		TblTrade* tblTrade = newTblTrade(o, curlObj);
		strcpy(tblTrade->AccountID, getBaseInfo("AccountID"));
		tblTradeList.push_back(tblTrade);
	}

	if (tblTradeList.size() > 0) {
		*pTblTrade = new TblTrade*[tblTradeList.size()];
		std::copy(tblTradeList.begin(), tblTradeList.end(), *pTblTrade);
	}

	curlObj->setRefreshInterval(atol(GetOpenedTradesInfo("Refresh")));
	return tblTradeList.size();
}

int COrder2Rest::getClosedTrades(TblTrade** pTblTrade[])
{
	CCurlImpl* curlObj = m_CurlList[CURL_GET_CLOSEDTRADES];
	CURLcode ret = curlObj->doEasyPerform();
	if (ret != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, curl_easy_strerror(ret));
		return RET_FAILED;
	}

	picojson::value json;
	picojson::array& list = parseJsonArray(curlObj, json);
	if (list.empty()) {
		curlObj->setRefreshInterval(atol(GetClosedTradesInfo("Refresh")));
		return 0;
	}

	vector<TblTrade*> tblTradeList;
	time_t weekFirstDay = CUtils::getWeekFirstDate();
	for (picojson::array::iterator it = list.begin(); it != list.end(); it++) {
		picojson::object& o = it->get<picojson::object>();
		TblTrade* tblTrade = newTblTrade(o, curlObj);
		if (tblTrade->CloseTime > weekFirstDay) {
			strcpy(tblTrade->AccountID, getBaseInfo("AccountID"));
			tblTradeList.push_back(tblTrade);
		}
		else {
			delete tblTrade;
		}
	}

	if (tblTradeList.size() > 0) {
		*pTblTrade = new TblTrade*[tblTradeList.size()];
		std::copy(tblTradeList.begin(), tblTradeList.end(), *pTblTrade);
	}

	curlObj->setRefreshInterval(atol(GetClosedTradesInfo("Refresh")));
	return tblTradeList.size();
}

int COrder2Rest::getHistoricalData(const char* symbol, const char* period, time_t start, time_t end, bool maxRange, TblCandle** pTblCandle[])
{
	// UTC (Sunday 19:00 - Friday 21:00)
	int marketOpenWday = atoi(getMarketInfo("OpenWday", "0"));
	int marketOpenHour = atoi(getMarketInfo("OpenHour", "19"));
	int marketCloseWday = atoi(getMarketInfo("CloseWday", "5"));
	int marketCloseHour = atoi(getMarketInfo("CloseHour", "21"));
	time_t interval = getTimetByPeriod(period);
	int adjustmentTimezone = getAdjustmentTimezone();	// 2023/10/09 add by yld

	vector<TblCandle*> outCandleList;
	time_t cur = start - interval;
	time_t addLastTime = start;

	while (cur <= end) {
		time_t to = cur + CandleMaxNumber * interval;
		time_t tmStdTime = m_tmStdTime.load();
		if (to > tmStdTime && tmStdTime > 0) {
			to = tmStdTime;
		}
		if (cur > to) {
			break;
		}
		
		vector<TblCandle*> tmpCandleList;
		if (getHistoricalData(symbol, period, cur, to, adjustmentTimezone, tmpCandleList) < 0) {
			return RET_FAILED;
		}
		
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

int COrder2Rest::openMarketOrder(TblOrder* tblOrder)
{
	vector<const char*> headers;
	getHeaders(headers);

	CCurlImpl m_curlTradeImpl(getBaseInfo("Host"), getSslVerify());
	m_curlTradeImpl.addHeaders(headers);
	m_curlTradeImpl.setPath(getOpenMarketOrderInfo("Path"), m_mapPathParams);
	m_curlTradeImpl.parseResFileds(getOpenMarketOrderInfo("Response"));
	if (m_curlTradeImpl.init(
		getOpenMarketOrderInfo("Method"), getOpenMarketOrderInfo("Request"), false) != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, "Can't init [OpenMarketOrder] curl.");
		return RET_FAILED;
	}

	vector<ReqParam> params;
	params.push_back(ReqParam{"$symbol", transfSymbol(tblOrder->Symbol)});
	string side = getSideInfo(tblOrder->BS);
	if (side.length() > 0) {
		params.push_back(ReqParam{"$amount", std::to_string((long)tblOrder->Amount)});
		params.push_back(ReqParam{"$bs", side});
	}
	else {
		if (strcmp(tblOrder->BS, "B") == 0) {
			params.push_back(ReqParam{"$amount", std::to_string(tblOrder->Amount)});
		}
		else {
			params.push_back(ReqParam{"$amount", std::to_string(tblOrder->Amount * -1)});
		}
	}
	m_curlTradeImpl.setEasyPerform(&params);

	CURLcode ret = m_curlTradeImpl.doEasyPerform();
	if (ret != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, curl_easy_strerror(ret));
		return RET_FAILED;
	}
	
	picojson::value json;
	picojson::object& obj = parseJsonObject(&m_curlTradeImpl, json);
	if (obj.empty()) {
		return RET_FAILED;
	}

	TblOrder* resOrder = newTblOrder(obj, &m_curlTradeImpl);
	m_pPluginProxy->onOrder(TableStatus::ST_NEW, resOrder);

	TblTrade* openedTrade = newTblTrade(obj, &m_curlTradeImpl);

	if (tblOrder->Stop != 0) {
		TblOrder* stopLossOrder = new TblOrder();
		strcpy(stopLossOrder->TradeID, resOrder->TradeID);
		strcpy(stopLossOrder->Symbol, tblOrder->Symbol);
		stopLossOrder->Stop = tblOrder->Stop;
		if (openStopLossOrder(stopLossOrder) == RET_SUCCESS) {
			strcpy(openedTrade->StopOrderID, stopLossOrder->OrderID);
			openedTrade->Stop = stopLossOrder->Stop;
		}
		delete stopLossOrder;
	}

	if (tblOrder->Limit != 0) {
		TblOrder* takeProfitOrder = new TblOrder();
		strcpy(takeProfitOrder->TradeID, resOrder->TradeID);
		strcpy(takeProfitOrder->Symbol, tblOrder->Symbol);
		takeProfitOrder->Limit = tblOrder->Limit;
		if (openTakeProfitOrder(takeProfitOrder) == RET_SUCCESS) {
			strcpy(openedTrade->LimitOrderID, takeProfitOrder->OrderID);
			openedTrade->Limit = takeProfitOrder->Limit;
		}
		delete takeProfitOrder;
	}

	strcpy(openedTrade->OpenOrderID, resOrder->OrderID);
	m_pPluginProxy->onOpenedTrade(TableStatus::ST_NEW, openedTrade);
	delete openedTrade;

	strcpy(tblOrder->OrderID, resOrder->OrderID);
	delete resOrder;
	return RET_SUCCESS;
}

int COrder2Rest::openStopLossOrder(TblOrder* tblOrder)
{
	vector<const char*> headers;
	getHeaders(headers);

	CCurlImpl m_curlTradeImpl(getBaseInfo("Host"), getSslVerify());
	m_curlTradeImpl.addHeaders(headers);
	m_curlTradeImpl.setPath(getStopLossOrderInfo("Path"), m_mapPathParams);
	m_curlTradeImpl.parseResFileds(getStopLossOrderInfo("Response"));
	if (m_curlTradeImpl.init(
		getStopLossOrderInfo("Method"), getStopLossOrderInfo("Request"), false) != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, "Can't init [openStopLossOrder] curl.");
		return RET_FAILED;
	}

	vector<ReqParam> params;
	params.push_back(ReqParam{"$stop", std::to_string(tblOrder->Stop)});
	params.push_back(ReqParam{"$trade_id", tblOrder->TradeID});
	m_curlTradeImpl.setEasyPerform(&params);

	CURLcode ret = m_curlTradeImpl.doEasyPerform();
	if (ret != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, curl_easy_strerror(ret));
		return RET_FAILED;
	}

	picojson::value json;
	picojson::object& obj = parseJsonObject(&m_curlTradeImpl, json);
	if (obj.empty()) {
		return RET_FAILED;
	}

	TblOrder* resOrder = newTblOrder(obj, &m_curlTradeImpl);
	if (strlen(resOrder->Symbol) == 0) {
		strcpy(resOrder->Symbol, tblOrder->Symbol);
	}
	m_pPluginProxy->onOrder(TableStatus::ST_NEW, resOrder);
	strcpy(tblOrder->OrderID, resOrder->OrderID);
	tblOrder->Stop = resOrder->Stop;
	delete resOrder;

	return RET_SUCCESS;
}

int COrder2Rest::openTakeProfitOrder(TblOrder* tblOrder)
{
	vector<const char*> headers;
	getHeaders(headers);

	CCurlImpl m_curlTradeImpl(getBaseInfo("Host"), getSslVerify());
	m_curlTradeImpl.addHeaders(headers);
	m_curlTradeImpl.setPath(getTakeProfitOrderInfo("Path"), m_mapPathParams);
	m_curlTradeImpl.parseResFileds(getTakeProfitOrderInfo("Response"));
	if (m_curlTradeImpl.init(
		getTakeProfitOrderInfo("Method"), getTakeProfitOrderInfo("Request"), false) != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, "Can't init [openTakeProfitOrder] curl.");
		return RET_FAILED;
	}

	vector<ReqParam> params;
	params.push_back(ReqParam{"$limit", std::to_string(tblOrder->Limit)});
	params.push_back(ReqParam{"$trade_id", tblOrder->TradeID});
	m_curlTradeImpl.setEasyPerform(&params);

	CURLcode ret = m_curlTradeImpl.doEasyPerform();
	if (ret != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, curl_easy_strerror(ret));
		return RET_FAILED;
	}

	picojson::value json;
	picojson::object& obj = parseJsonObject(&m_curlTradeImpl, json);
	if (obj.empty()) {
		return RET_FAILED;
	}

	TblOrder* resOrder = newTblOrder(obj, &m_curlTradeImpl);
	if (strlen(resOrder->Symbol) == 0) {
		strcpy(resOrder->Symbol, tblOrder->Symbol);
	}
	m_pPluginProxy->onOrder(TableStatus::ST_NEW, resOrder);
	strcpy(tblOrder->OrderID, resOrder->OrderID);
	tblOrder->Limit = resOrder->Limit;
	delete resOrder;

	return RET_SUCCESS;
}

int COrder2Rest::changeStopLoss(TblTrade* tblTrade)
{
	vector<const char*> headers;
	getHeaders(headers);

	CCurlImpl m_curlTradeImpl(getBaseInfo("Host"), getSslVerify());
	m_curlTradeImpl.addHeaders(headers);
	m_mapPathParams["$order_id"] = tblTrade->StopOrderID;
	m_curlTradeImpl.setPath(getChangeStopLossInfo("Path"), m_mapPathParams);
	m_curlTradeImpl.parseResFileds(getChangeStopLossInfo("Response"));
	if (m_curlTradeImpl.init(
		getChangeStopLossInfo("Method"), getChangeStopLossInfo("Request"), false) != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, "Can't init [changeStop] curl.");
		return RET_FAILED;
	}

	vector<ReqParam> params;
	params.push_back(ReqParam{"$stop", std::to_string(tblTrade->Stop)});
	params.push_back(ReqParam{"$trade_id", tblTrade->TradeID});
	m_curlTradeImpl.setEasyPerform(&params);

	CURLcode ret = m_curlTradeImpl.doEasyPerform();
	if (ret != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, curl_easy_strerror(ret));
		return RET_FAILED;
	}

	picojson::value json;
	picojson::object& obj = parseJsonObject(&m_curlTradeImpl, json);
	if (obj.empty()) {
		return RET_FAILED;
	}

	TblOrder* resOrder = newTblOrder(obj, &m_curlTradeImpl);
	if (strlen(resOrder->Symbol) == 0) {
		strcpy(resOrder->Symbol, tblTrade->Symbol);
	}
	m_pPluginProxy->onOrder(TableStatus::ST_NEW, resOrder);
	strcpy(tblTrade->StopOrderID, resOrder->OrderID);
	delete resOrder;

	return RET_SUCCESS;
}

int COrder2Rest::changeTakeProfit(TblTrade* tblTrade)
{
	vector<const char*> headers;
	getHeaders(headers);

	CCurlImpl m_curlTradeImpl(getBaseInfo("Host"), getSslVerify());
	m_curlTradeImpl.addHeaders(headers);
	m_mapPathParams["$order_id"] = tblTrade->LimitOrderID;
	m_curlTradeImpl.setPath(getChangeTakeProfitInfo("Path"), m_mapPathParams);
	m_curlTradeImpl.parseResFileds(getChangeTakeProfitInfo("Response"));
	if (m_curlTradeImpl.init(
		getChangeTakeProfitInfo("Method"), getChangeTakeProfitInfo("Request"), false) != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, "Can't init [changeLimit] curl.");
		return RET_FAILED;
	}

	vector<ReqParam> params;
	params.push_back(ReqParam{"$limit", std::to_string(tblTrade->Limit)});
	params.push_back(ReqParam{"$trade_id", tblTrade->TradeID});
	m_curlTradeImpl.setEasyPerform(&params);

	CURLcode ret = m_curlTradeImpl.doEasyPerform();
	if (ret != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, curl_easy_strerror(ret));
		return RET_FAILED;
	}

	picojson::value json;
	picojson::object& obj = parseJsonObject(&m_curlTradeImpl, json);
	if (obj.empty()) {
		return RET_FAILED;
	}

	TblOrder* resOrder = newTblOrder(obj, &m_curlTradeImpl);
	if (strlen(resOrder->Symbol) == 0) {
		strcpy(resOrder->Symbol, tblTrade->Symbol);
	}
	m_pPluginProxy->onOrder(TableStatus::ST_NEW, resOrder);
	strcpy(tblTrade->LimitOrderID, resOrder->OrderID);
	delete resOrder;

	return RET_SUCCESS;
}

int COrder2Rest::closeTrade(TblTrade* tblTrade)
{
	vector<const char*> headers;
	getHeaders(headers);

	CCurlImpl m_curlTradeImpl(getBaseInfo("Host"), getSslVerify());
	m_curlTradeImpl.addHeaders(headers);
	m_mapPathParams["$trade_id"] = tblTrade->TradeID;
	m_curlTradeImpl.setPath(getCloseTradeInfo("Path"), m_mapPathParams);
	m_curlTradeImpl.parseResFileds(getCloseTradeInfo("Response"));
	if (m_curlTradeImpl.init(
		getCloseTradeInfo("Method"), getCloseTradeInfo("Request"), false) != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, "Can't init [CloseTrade] curl.");
		return RET_FAILED;
	}

	vector<ReqParam> params;
	params.push_back(ReqParam{"$amount", std::to_string((long)tblTrade->Amount)});
	m_curlTradeImpl.setEasyPerform(&params);

	CURLcode ret = m_curlTradeImpl.doEasyPerform();
	if (ret != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, curl_easy_strerror(ret));
		return RET_FAILED;
	}

	picojson::value json;
	picojson::object& obj = parseJsonObject(&m_curlTradeImpl, json);
	if (obj.empty()) {
		return RET_FAILED;
	}

	TblOrder* resOrder = newTblOrder(obj, &m_curlTradeImpl);
	strcpy(resOrder->TradeID, tblTrade->TradeID);
	m_pPluginProxy->onOrder(TableStatus::ST_NEW, resOrder);
	delete resOrder;

	TblTrade* closedTrade = newTblTrade(obj, &m_curlTradeImpl);
	strcpy(closedTrade->TradeID, tblTrade->TradeID);
	closedTrade->Open = tblTrade->Open;
	closedTrade->Stop = tblTrade->Stop;
	closedTrade->Limit = tblTrade->Limit;
	closedTrade->High = tblTrade->High;
	closedTrade->Low = tblTrade->Low;
	closedTrade->OpenTime = tblTrade->OpenTime;
	strcpy(closedTrade->OpenOrderID, tblTrade->OpenOrderID);
	strcpy(closedTrade->StopOrderID, tblTrade->StopOrderID);
	strcpy(closedTrade->LimitOrderID, tblTrade->LimitOrderID);
	m_pPluginProxy->onOpenedTrade(TableStatus::ST_DEL, closedTrade);
	m_pPluginProxy->onClosedTrade(TableStatus::ST_NEW, closedTrade);
	delete closedTrade;

	return RET_SUCCESS;
}

int COrder2Rest::initCurl()
{
	bool sslVerify = getSslVerify();
	vector<const char*> headers;
	getHeaders(headers);

	// ==== GetPrice Curl init ====
	m_CurlList[CURL_GET_PRICE] = new CCurlImpl(getBaseInfo("Host"), sslVerify);
	m_CurlList[CURL_GET_PRICE]->addHeaders(headers);
	m_CurlList[CURL_GET_PRICE]->setPath(getPriceInfo("Path"), m_mapPathParams);
	m_CurlList[CURL_GET_PRICE]->parseResFileds(getPriceInfo("Response"));
	if (m_CurlList[CURL_GET_PRICE]->init(
		getPriceInfo("Method"), getPriceInfo("Request"), true, onGetPrice) == CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_DEBUG, "[GetPrice] curl_easy_init succeeded.");
	}
	else {
		m_pPluginProxy->onMessage(MSG_ERROR, "Can't init [GetPrice] curl.");
		return RET_FAILED;
	}
	m_CurlList[CURL_GET_PRICE]->setEasyPerform();

	// ==== GetAccount Curl init ====
	m_CurlList[CURL_GET_ACCOUNT] = new CCurlImpl(getBaseInfo("Host"), sslVerify);
	m_CurlList[CURL_GET_ACCOUNT]->addHeaders(headers);
	m_CurlList[CURL_GET_ACCOUNT]->setPath(getAccountInfo("Path"), m_mapPathParams);
	m_CurlList[CURL_GET_ACCOUNT]->parseResFileds(getAccountInfo("Response"));
	if (m_CurlList[CURL_GET_ACCOUNT]->init(
		getAccountInfo("Method"), getAccountInfo("Request"), false, onGetAccount) == CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_DEBUG, "[GetAccount] curl_easy_init succeeded.");
	}
	else {
		m_pPluginProxy->onMessage(MSG_ERROR, "Can't init [GetAccount] curl.");
		return RET_FAILED;
	}
	m_CurlList[CURL_GET_ACCOUNT]->setEasyPerform();

	// ==== GetOpenedTrades Curl init ====
	m_CurlList[CURL_GET_OPENTRADES] = new CCurlImpl(getBaseInfo("Host"), sslVerify);
	m_CurlList[CURL_GET_OPENTRADES]->addHeaders(headers);
	m_CurlList[CURL_GET_OPENTRADES]->setPath(GetOpenedTradesInfo("Path"), m_mapPathParams);
	m_CurlList[CURL_GET_OPENTRADES]->parseResFileds(GetOpenedTradesInfo("Response"));
	if (m_CurlList[CURL_GET_OPENTRADES]->init(
		GetOpenedTradesInfo("Method"), GetOpenedTradesInfo("Request"), false, onGetOpenTrades) == CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_DEBUG, "[GetOpenedTrades] curl_easy_init succeeded.");
	}
	else {
		m_pPluginProxy->onMessage(MSG_ERROR, "Can't init [GetOpenedTrades] curl.");
		return RET_FAILED;
	}
	m_CurlList[CURL_GET_OPENTRADES]->setEasyPerform();

	// ==== GetClosedTrades Curl init ====
	m_CurlList[CURL_GET_CLOSEDTRADES] = new CCurlImpl(getBaseInfo("Host"), sslVerify);
	m_CurlList[CURL_GET_CLOSEDTRADES]->addHeaders(headers);
	m_CurlList[CURL_GET_CLOSEDTRADES]->setPath(GetClosedTradesInfo("Path"), m_mapPathParams);
	m_CurlList[CURL_GET_CLOSEDTRADES]->parseResFileds(GetClosedTradesInfo("Response"));
	if (m_CurlList[CURL_GET_CLOSEDTRADES]->init(
		GetClosedTradesInfo("Method"), GetClosedTradesInfo("Request"), false, onGetClosedTrades) == CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_DEBUG, "[GetClosedTrades] curl_easy_init succeeded.");
	}
	else {
		m_pPluginProxy->onMessage(MSG_ERROR, "Can't init [GetClosedTrades] curl.");
		return RET_FAILED;
	}
	m_CurlList[CURL_GET_CLOSEDTRADES]->setEasyPerform();

	// ==== GetHistoricalData Curl init ====
	m_CurlList[CURL_GET_CANDLES] = new CCurlImpl(getBaseInfo("Host"), sslVerify);
	m_CurlList[CURL_GET_CANDLES]->addHeaders(headers);
	m_CurlList[CURL_GET_CANDLES]->parseResFileds(GetHistoricalDataInfo("Response"));
	if (m_CurlList[CURL_GET_CANDLES]->init(
		GetHistoricalDataInfo("Method"), GetHistoricalDataInfo("Request"), false) == CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_DEBUG, "[GetHistoricalData] curl_easy_init succeeded.");
	}
	else {
		m_pPluginProxy->onMessage(MSG_ERROR, "Can't init [GetHistoricalData] curl.");
		return RET_FAILED;
	}
	m_CurlList[CURL_GET_CANDLES]->setEasyPerform();

	return RET_SUCCESS;
}

int COrder2Rest::startTradeEventThread()
{
	return m_pTradeEventsProcessThread->_start();
}

void COrder2Rest::tradeEventsProcess(void *pv)
{
	COrder2Rest* order2Rest = (COrder2Rest*)pv;
	order2Rest->m_pPluginProxy->onMessage(MSG_DEBUG, "Order2Rest event thread begin...");
	order2Rest->waitNextEvent();
	order2Rest->m_pPluginProxy->onMessage(MSG_DEBUG, "Order2Rest event thread end.");
}

void COrder2Rest::waitNextEvent()
{
	while (true) {
		DWORD dwRes = nsapi::WaitForSingleObject(m_hExitEvent, 100);
		if (dwRes == WAIT_OBJECT_0) {
			nsapi::SetEvent(m_hOverEvent);
			break;
		}
		else if (dwRes == WAIT_TIMEOUT) {
			onTableListener();
		}
	}
}

void COrder2Rest::onTableListener()
{
	if (m_pCurlMulti) {

		for (int i = 0; i < sizeof(m_CurlList) / sizeof(m_CurlList[0]); i++) {
			if (m_CurlList[i]->getResListener() && m_CurlList[i]->chkRefresh()) {
				m_CurlList[i]->clear();
				curl_multi_add_handle(m_pCurlMulti, m_CurlList[i]->getCurlHandle());
			}
		}

		int running;
		curl_multi_perform(m_pCurlMulti, &running);
		do {
			struct timeval timeout;
			long multi_timeout = -1;

			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			curl_multi_timeout(m_pCurlMulti, &multi_timeout);
			if(multi_timeout >= 0) {
				timeout.tv_sec = multi_timeout / 1000;
				if(timeout.tv_sec > 1)
					timeout.tv_sec = 1;
				else
					timeout.tv_usec = (multi_timeout % 1000) * 1000;
			}
 
			fd_set fdread;
			fd_set fdwrite;
			fd_set fdexcep;
			FD_ZERO(&fdread);
			FD_ZERO(&fdwrite);
			FD_ZERO(&fdexcep);

			int maxfd = -1;
			CURLMcode mc = curl_multi_fdset(m_pCurlMulti, &fdread, &fdwrite, &fdexcep, &maxfd);
			if(mc != CURLM_OK) {
				string s = "curl_multi_fdset failed, code: " + std::to_string(mc);
				m_pPluginProxy->onMessage(MSG_ERROR, s.c_str());
				break;
			}

			int rc = 0;
			if(maxfd == -1) {
				nsapi::Sleep(100);
			}
			else {
				rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);
			}
 
			switch(rc) {
			case -1:
				break;
			case 0:
			default:
				curl_multi_perform(m_pCurlMulti, &running);
				break;
			}
		} while(running);

		CURLMsg *msg;
		int msgs_left;
		while((msg = curl_multi_info_read(m_pCurlMulti, &msgs_left))) {
			if(msg->msg == CURLMSG_DONE) {
				for (int i = 0; i < sizeof(m_CurlList) / sizeof(m_CurlList[0]); i++) {
					if (msg->easy_handle == m_CurlList[i]->getCurlHandle()) {
						m_CurlList[i]->onResListener(this);
						curl_multi_remove_handle(m_pCurlMulti, msg->easy_handle);
						break;
					}
				}
			}
		}

	}
	else {
		for (int i = 0; i < sizeof(m_CurlList) / sizeof(m_CurlList[0]); i++) {
			if (m_CurlList[i]->getResListener() && m_CurlList[i]->chkRefresh()) {
				if (m_CurlList[i]->doEasyPerform() == CURLE_OK) {
					m_CurlList[i]->onResListener(this);
				}
			}
		}
	}
}

void COrder2Rest::onGetPrice(void* curlobj, void* listener)
{
	CCurlImpl* curlObj = (CCurlImpl*)curlobj;
	COrder2Rest* order2Rest = (COrder2Rest*)listener;

	picojson::value json;
	picojson::array& list = order2Rest->parseJsonArray(curlObj, json);
	if (list.empty()) {
		return;
	}

	for (picojson::array::iterator it = list.begin(); it != list.end(); it++) {
		picojson::object& o = it->get<picojson::object>();
		TblPrice* tblPrice = order2Rest->newTblPrice(o, curlObj);
		order2Rest->m_pPluginProxy->onPrice(TableStatus::ST_UPD, tblPrice);
		delete tblPrice;
	}

	order2Rest->m_tmStdTime.store(order2Rest->reqServerTime());
}

void COrder2Rest::onGetAccount(void* curlobj, void* listener)
{
	CCurlImpl* curlObj = (CCurlImpl*)curlobj;
	COrder2Rest* order2Rest = (COrder2Rest*)listener;

	picojson::value json;
	picojson::object& obj = order2Rest->parseJson(curlObj, json);
	if (obj.empty()) {
		return;
	}

	TblAccount *tblAccount = order2Rest->newTblAccount(obj, curlObj);
	if (strcmp(tblAccount->AccountID, order2Rest->getBaseInfo("AccountID")) == 0) {
		order2Rest->m_pPluginProxy->onAccount(TableStatus::ST_UPD, tblAccount);
	}
	delete tblAccount;
}

void COrder2Rest::onGetOpenTrades(void* curlobj, void* listener)
{
	CCurlImpl* curlObj = (CCurlImpl*)curlobj;
	COrder2Rest* order2Rest = (COrder2Rest*)listener;
	
	picojson::value json;
	picojson::array& list = order2Rest->parseJsonArray(curlObj, json);
	if (list.empty()) {
		return;
	}

	for (picojson::array::iterator it = list.begin(); it != list.end(); it++) {
		picojson::object& o = it->get<picojson::object>();
		TblTrade* tblTrade = order2Rest->newTblTrade(o, curlObj);
		order2Rest->m_pPluginProxy->onOpenedTrade(TableStatus::ST_UPD, tblTrade);
		delete tblTrade;
	}
}
	
void COrder2Rest::onGetClosedTrades(void* curlobj, void* listener)
{
	CCurlImpl* curlObj = (CCurlImpl*)curlobj;
	COrder2Rest* order2Rest = (COrder2Rest*)listener;

	picojson::value json;
	picojson::array& list = order2Rest->parseJsonArray(curlObj, json);
	if (list.empty()) {
		return;
	}

	time_t weekFirstDay = CUtils::getWeekFirstDate();
	for (picojson::array::iterator it = list.begin(); it != list.end(); it++) {
		picojson::object& o = it->get<picojson::object>();
		TblTrade* tblTrade = order2Rest->newTblTrade(o, curlObj);
		if (tblTrade->CloseTime > weekFirstDay) {
			order2Rest->m_pPluginProxy->onClosedTrade(TableStatus::ST_UPD, tblTrade);
		}
		delete tblTrade;
	}
}

int COrder2Rest::onJsonError(picojson::object& o)
{
	string message;
	if (COrder2Rest::json2Str(o, getErrorInfo("Message"), message)) {
		message = "[onJsonError] " + message;
		m_pPluginProxy->onMessage(MSG_ERROR, message.c_str());
		return RET_FAILED;
	}
	return RET_SUCCESS;
}

picojson::object& COrder2Rest::parseJson(CCurlImpl* curlImpl, picojson::value& json)
{
	static picojson::object nullopt;

	if (curlImpl->getResSize() == 0) {
		return nullopt;
	}

	string err;
	picojson::parse(json, curlImpl->getResContents(),
		curlImpl->getResContents() + curlImpl->getResSize(), &err);
	if (!err.empty() || json.is<picojson::null>()) {
		err = err + "\n" + curlImpl->getResContents();
		m_pPluginProxy->onMessage(MSG_ERROR, err.c_str());
		return nullopt;
	}

	picojson::object& obj = json.get<picojson::object>();
	if (onJsonError(obj) == RET_FAILED) {
		m_pPluginProxy->onMessage(MSG_ERROR, curlImpl->toString().c_str());
		return nullopt;
	}

	return obj;
}

picojson::object& COrder2Rest::parseJsonObject(CCurlImpl* curlImpl, picojson::value& json)
{
	static picojson::object nullopt;

	picojson::object& obj = parseJson(curlImpl, json);
	if (obj.empty()) {
		return nullopt;
	}
	string resTargetName = curlImpl->getResField(CCurlImpl::ResTargetName);
	if (resTargetName.empty()) {
		return obj;
	}
	map<string, picojson::value>::iterator mpos = obj.find(resTargetName);
	if (mpos == obj.end()) {
		string s = "Can't find object " + resTargetName;
		m_pPluginProxy->onMessage(MSG_ERROR, s.c_str());
		return nullopt;
	}
	return mpos->second.get<picojson::object>();
}

picojson::array& COrder2Rest::parseJsonArray(CCurlImpl* curlImpl, picojson::value& json)
{
	static picojson::array nullopt;

	picojson::object& obj = parseJson(curlImpl, json);
	if (obj.empty()) {
		return nullopt;
	}
	
	string resTargetName = curlImpl->getResField(CCurlImpl::ResTargetName);
	map<string, picojson::value>::iterator mpos = obj.find(resTargetName);
	if (mpos == obj.end()) {
		string s = "Can't find array " + resTargetName;
		m_pPluginProxy->onMessage(MSG_ERROR, s.c_str());
		return nullopt;
	}
	if (!mpos->second.is<picojson::array>()) {
		string s = resTargetName + " is not array.";
		m_pPluginProxy->onMessage(MSG_ERROR, s.c_str());
		return nullopt;
	}
	return mpos->second.get<picojson::array>();
}

time_t COrder2Rest::reqServerTime()
{
	long resCode;
	CURLcode res = curl_easy_getinfo(m_CurlList[CURL_GET_PRICE]->getCurlHandle(), CURLINFO_RESPONSE_CODE, &resCode);
	if (res == CURLE_OK && resCode == 200) {
		return CUtils::HStr2Time(m_CurlList[CURL_GET_PRICE]->getResHeader());
	}
	return 0;
}

int COrder2Rest::getHistoricalData(const char* symbol, const char* period, time_t start, time_t end, int adjustmentTimezone, vector<TblCandle*>& tblCandleList)
{
	CCurlImpl* curlObj = m_CurlList[CURL_GET_CANDLES];
	
	m_mapPathParams["$symbol"] = transfSymbol(symbol);
	curlObj->setPath(GetHistoricalDataInfo("Path"), m_mapPathParams);

	vector<ReqParam> params;
	params.push_back(ReqParam{"$symbol", transfSymbol(symbol)});
	params.push_back(ReqParam{"$period", transfPeriod(period)});
	params.push_back(ReqParam{"$start", transfTime(start + adjustmentTimezone) });
	params.push_back(ReqParam{"$end", transfTime(end + adjustmentTimezone) });
	curlObj->setEasyPerform(&params);

	CURLcode ret = curlObj->doEasyPerform();
	if (ret != CURLE_OK) {
		m_pPluginProxy->onMessage(MSG_ERROR, curl_easy_strerror(ret));
		return RET_FAILED;
	}

	picojson::value json;
	picojson::array& list = parseJsonArray(curlObj, json);
	if (list.empty()) {
		return 0;
	}
	for (picojson::array::iterator it = list.begin(); it != list.end(); it++) {
		picojson::object& o = it->get<picojson::object>();
		TblCandle* tblCandle = COrder2Rest::newTblCandle(o, curlObj);
		strcpy(tblCandle->Symbol, symbol);
		strcpy(tblCandle->Period, period);
		tblCandleList.push_back(tblCandle);
	}

	char buf[256];
	tm tmStart = CUtils::getUTCCal(start);
	tm tmEnd = CUtils::getUTCCal(end);
	tm tmOutStart = CUtils::getUTCCal(tblCandleList.front()->StartDate);
	tm tmOutEnd = CUtils::getUTCCal(tblCandleList.back()->StartDate);
	sprintf(buf, "[GetHistoricalData] Count:%d StartDate:%s EndDate:%s OutStartDate:%s OutEndDate:%s",
		tblCandleList.size(),
		CUtils::strOfTime(&tmStart, TimeFormat).c_str(),
		CUtils::strOfTime(&tmEnd, TimeFormat).c_str(),
		CUtils::strOfTime(&tmOutStart, TimeFormat).c_str(),
		CUtils::strOfTime(&tmOutEnd, TimeFormat).c_str());
	m_pPluginProxy->onMessage(MSG_DEBUG, buf);

	return tblCandleList.size();
}

bool COrder2Rest::getSslVerify()
{
	return strcmp(getBaseInfo("SslVerify"), "true") == 0 ? true : false;
}

void COrder2Rest::getHeaders(vector<const char*>& headers)
{
	CSimpleIniCaseA::TNamesDepend keys;
	m_SimpleIni.GetAllKeys("Header", keys);
	CSimpleIniCaseA::TNamesDepend::const_iterator it = keys.begin();
	while (it != keys.end()) {
		headers.push_back(getHeaderInfo(it->pItem));
		it++;
	}
}

// 2023/10/09 add by yld
int COrder2Rest::getAdjustmentTimezone()
{
	return std::stoi(getBaseInfo("AdjustmentTimezone", 0)) * 3600;
}

string COrder2Rest::transfSymbol(const char* symbol)
{
	string s = symbol;
	CUtils::replace(s, "/", getSymbolInfo("Combination"));
	return s;
}

string COrder2Rest::transfSymbols(const char* symbols[])
{
	string s;
	for (int i = 0; symbols[i]; i++) {
		if (s.empty()) {
			s = transfSymbol(symbols[i]);
		}
		else {
			s = s + getSymbolInfo("Delimiter") + transfSymbol(symbols[i]);
		}
	}
	return s;
}

string COrder2Rest::transfPeriod(const char* period)
{
	return getPeriodInfo(period);
}

string COrder2Rest::transfSide(const char* side)
{
	if (strlen(side) == 0) {
		return "";
	}
	if (strcmp(side, getSideInfo("B")) == 0) {
		return "B";
	}
	return "S";
}

string COrder2Rest::transfSide(double amount)
{
	if (amount == 0) {
		return "";
	}
	if (amount > 0) {
		return "B";
	}
	return "S";
}

string COrder2Rest::transfTime(time_t t)
{
	string s;
	if (strcmp(getBaseInfo("TimeFormat"), "RFC3339") == 0) {
		s = CUtils::strOfTimeWithRFC3339(t);
	}
	else {
		s = CUtils::strOfTime(t, TimeFormat);
	}
	CUtils::replace(s, ":", "%3A");
	return s;
}

picojson::value& COrder2Rest::findJsonValue(picojson::object& o, const char* key)
{
	static picojson::value nullopt;

	vector<string> keys;
	char* s = strdup(key);
	char* p = s;
	for (int i = 0; s[i]; i++) {
		if (s[i] == '.') {
			s[i] = '\0';
			keys.push_back(p);
			p += (strlen(p) + 1);
		}
	}
	keys.push_back(p);
	free(s);

	picojson::object* cur = &o;
	for (int i = 0; i < keys.size() - 1; i++) {
		map<string, picojson::value>::iterator it = cur->find(keys[i]);
		if (it == cur->end()) {
			return nullopt;
		}
		cur = &it->second.get<picojson::object>();
	}

	map<string, picojson::value>::iterator it = cur->find(keys.back());
	if (it == cur->end()) {
		return nullopt;
	}
	return it->second;
}

bool COrder2Rest::json2Bool(picojson::object& o, const char* key, bool* val)
{
	picojson::value& jval = findJsonValue(o, key);
	if (jval.is<picojson::null>()) {
		*val = false;
		return false;
	}
	if (jval.is<bool>()) {
		*val = jval.get<bool>();
	}
	else if (jval.is<double>()) {
		*val = (bool)jval.get<double>();
	}
	else if (jval.is<string>()) {
		*val = stricmp(jval.get<string>().c_str(), "true") == 0 ? true : false;
	}
	return true;
}

bool COrder2Rest::json2Int(picojson::object& o, const char* key, int* val)
{
	picojson::value& jval = findJsonValue(o, key);
	if (jval.is<picojson::null>()) {
		*val = 0;
		return false;
	}
	if (jval.is<bool>()) {
		*val = (int)jval.get<bool>();
	}
	else if (jval.is<double>()) {
		*val = (int)jval.get<double>();
	}
	else if (jval.is<string>()) {
		*val = atoi(jval.get<string>().c_str());
	}
	return true;
}

bool COrder2Rest::json2Lng(picojson::object& o, const char* key, long* val)
{
	picojson::value& jval = findJsonValue(o, key);
	if (jval.is<picojson::null>()) {
		*val = 0;
		return false;
	}
	if (jval.is<bool>()) {
		*val = (long)jval.get<bool>();
	}
	else if (jval.is<double>()) {
		*val = (long)jval.get<double>();
	}
	else if (jval.is<string>()) {
		*val = atol(jval.get<string>().c_str());
	}
	return true;
}

bool COrder2Rest::json2Dbl(picojson::object& o, const char* key, double* val)
{
	picojson::value& jval = findJsonValue(o, key);
	if (jval.is<picojson::null>()) {
		*val = 0;
		return false;
	}
	if (jval.is<bool>()) {
		*val = (double)jval.get<bool>();
	}
	else if (jval.is<double>()) {
		*val = jval.get<double>();
	}
	else if (jval.is<string>()) {
		*val = atof(jval.get<string>().c_str());
	}
	return true;
}

bool COrder2Rest::json2Str(picojson::object& o, const char* key, string& val)
{
	picojson::value& jval = findJsonValue(o, key);
	if (jval.is<picojson::null>()) {
		val = "";
		return false;
	}
	if (jval.is<bool>()) {
		val = jval.get<bool>() ? "true" : "false";
	}
	else if (jval.is<double>()) {
		val = std::to_string(jval.get<double>());
		string::size_type pos = val.find(".");
		if (pos != string::npos) {
			val = val.substr(0, pos);
		}
	}
	else if (jval.is<string>()) {
		val = jval.get<string>();
	}
	return true;
}

bool COrder2Rest::json2Time(picojson::object& o, const char* key, time_t* val)
{
	picojson::value& jval = findJsonValue(o, key);
	if (jval.is<picojson::null>()) {
		*val = 0;
		return false;
	}
	if (jval.is<bool>()) {
		*val = (time_t)jval.get<bool>();
	}
	else if (jval.is<double>()) {
		*val = (time_t)jval.get<double>();
	}
	else if (jval.is<string>()) {
		*val = CUtils::str2Time(jval.get<string>().c_str());
	}
	return true;
}

TblCandle* COrder2Rest::newTblCandle(picojson::object& o, CCurlImpl* curlImpl)
{
	TblCandle* tblCandle = new TblCandle();
	json2Time(o, curlImpl->getResField("StartDate").c_str(), &tblCandle->StartDate);
	json2Dbl(o, curlImpl->getResField("AskClose").c_str(), &tblCandle->AskClose);
	json2Dbl(o, curlImpl->getResField("AskHigh").c_str(), &tblCandle->AskHigh);
	json2Dbl(o, curlImpl->getResField("AskLow").c_str(), &tblCandle->AskLow);
	json2Dbl(o, curlImpl->getResField("AskOpen").c_str(), &tblCandle->AskOpen);
	json2Dbl(o, curlImpl->getResField("BidClose").c_str(), &tblCandle->BidClose);
	json2Dbl(o, curlImpl->getResField("BidHigh").c_str(), &tblCandle->BidHigh);
	json2Dbl(o, curlImpl->getResField("BidLow").c_str(), &tblCandle->BidLow);
	json2Dbl(o, curlImpl->getResField("BidOpen").c_str(), &tblCandle->BidOpen);
	return tblCandle;
}

TblAccount* COrder2Rest::newTblAccount(picojson::object& o, CCurlImpl* curlImpl)
{
	TblAccount* tblAccount = new TblAccount();
	string s;
	// AccountId
	json2Str(o, curlImpl->getResField("AccountID").c_str(), s);
	strcpy(tblAccount->AccountID, s.c_str());
	// AccountName
	json2Str(o, curlImpl->getResField("AccountName").c_str(), s);
	strcpy(tblAccount->AccountName, s.c_str());
	// AccountType
	tblAccount->AccountType[0] = '\0';
	// Balance
	json2Dbl(o, curlImpl->getResField("Balance").c_str(), &tblAccount->Balance);
	// DayPL
	json2Dbl(o, curlImpl->getResField("DayPL").c_str(), &tblAccount->DayPL);
	// GrossPL
	json2Dbl(o, curlImpl->getResField("GrossPL").c_str(), &tblAccount->GrossPL);
	// Equity
	json2Dbl(o, curlImpl->getResField("Equity").c_str(), &tblAccount->Equity);
	if (tblAccount->Equity == 0) {
		tblAccount->Equity = tblAccount->Balance + tblAccount->GrossPL;
	}
	// UsedMargin
	json2Dbl(o, curlImpl->getResField("UsedMargin").c_str(), &tblAccount->UsedMargin);
	// UsableMargin
	json2Dbl(o, curlImpl->getResField("UsableMargin").c_str(), &tblAccount->UsableMargin);
	// UsableMarginInPercent
	json2Dbl(o, curlImpl->getResField("UsableMarginInPercent").c_str(), &tblAccount->UsableMarginInPercent);
	// UsableMaintMarginInPercent
	json2Dbl(o, curlImpl->getResField("UsableMaintMarginInPercent").c_str(), &tblAccount->UsableMaintMarginInPercent);
	// MarginRate
	json2Dbl(o, curlImpl->getResField("MarginRate").c_str(), &tblAccount->MarginRate);
	// Hedging
	json2Str(o, curlImpl->getResField("Hedging").c_str(), s);
	strcpy(tblAccount->Hedging, s.c_str());
	// Currency
	json2Str(o, curlImpl->getResField("Currency").c_str(), s);
	strcpy(tblAccount->Currency, s.c_str());
	// Broker
	strcpy(tblAccount->Broker, getBaseInfo("Broker"));
	// Reserve
	json2Str(o, curlImpl->getResField("Reserve").c_str(), s);
	strcpy(tblAccount->Reserve, s.c_str());
	return tblAccount;
}

TblPrice* COrder2Rest::newTblPrice(picojson::object& o, CCurlImpl* curlImpl)
{
	TblPrice* tblPrice = new TblPrice();
	string s;
	// Symbol
	json2Str(o, curlImpl->getResField("Symbol").c_str(), s);
	CUtils::replace(s, getSymbolInfo("Combination"), "/");
	strcpy(tblPrice->Symbol, s.c_str());
	// SymbolType
	json2Str(o, curlImpl->getResField("SymbolType").c_str(), s);
	strcpy(tblPrice->SymbolType, s.c_str());
	// OfferID
	json2Str(o, curlImpl->getResField("OfferID").c_str(), s);
	strcpy(tblPrice->OfferID, s.c_str());
	// Bid
	json2Dbl(o, curlImpl->getResField("Bid").c_str(), &tblPrice->Bid);
	// Ask
	json2Dbl(o, curlImpl->getResField("Ask").c_str(), &tblPrice->Ask);
	// Time
	json2Time(o, curlImpl->getResField("Time").c_str(), &tblPrice->Time);
	// High
	json2Dbl(o, curlImpl->getResField("High").c_str(), &tblPrice->High);
	// Low
	json2Dbl(o, curlImpl->getResField("Low").c_str(), &tblPrice->Low);
	// PipCost
	json2Dbl(o, curlImpl->getResField("PipCost").c_str(), &tblPrice->PipCost);
	// PointSize
	json2Dbl(o, curlImpl->getResField("PointSize").c_str(), &tblPrice->PointSize);
	// Reserve
	json2Str(o, curlImpl->getResField("Reserve").c_str(), s);
	strcpy(tblPrice->Reserve, s.c_str());
	return tblPrice;
}

TblOrder* COrder2Rest::newTblOrder(picojson::object& o, CCurlImpl* curlImpl)
{
	TblOrder* tblOrder = new TblOrder();
	string s;
	// OrderID
	json2Str(o, curlImpl->getResField("OrderID").c_str(), s);
	strcpy(tblOrder->OrderID, s.c_str());
	// RequestID
	json2Str(o, curlImpl->getResField("RequestID").c_str(), s);
	strcpy(tblOrder->RequestID, s.c_str());
	// AccountID
	json2Str(o, curlImpl->getResField("AccountID").c_str(), s);
	strcpy(tblOrder->AccountID, s.c_str());
	// OfferID
	json2Str(o, curlImpl->getResField("OfferID").c_str(), s);
	strcpy(tblOrder->OfferID, s.c_str());
	// Symbol
	json2Str(o, curlImpl->getResField("Symbol").c_str(), s);
	CUtils::replace(s, getSymbolInfo("Combination"), "/");
	strcpy(tblOrder->Symbol, s.c_str());
	// TradeID
	json2Str(o, curlImpl->getResField("TradeID").c_str(), s);
	strcpy(tblOrder->TradeID, s.c_str());
	// Stage
	json2Str(o, curlImpl->getResField("Stage").c_str(), s);
	strcpy(tblOrder->Stage, s.c_str());
	// OrderType
	json2Str(o, curlImpl->getResField("OrderType").c_str(), s);
	strcpy(tblOrder->OrderType, s.c_str());
	// OrderStatus
	json2Str(o, curlImpl->getResField("OrderStatus").c_str(), s);
	strcpy(tblOrder->OrderStatus, s.c_str());
	// Amount
	json2Dbl(o, curlImpl->getResField("Amount").c_str(), &tblOrder->Amount);
	// BS
	json2Str(o, curlImpl->getResField("BS").c_str(), s);
	strcpy(tblOrder->BS, transfSide(s.c_str()).c_str());
	if (strlen(tblOrder->BS) == 0) {
		strcpy(tblOrder->BS, transfSide(tblOrder->Amount).c_str());
	}
	tblOrder->Amount = abs(tblOrder->Amount);
	// Rate
	json2Dbl(o, curlImpl->getResField("Rate").c_str(), &tblOrder->Rate);
	// Stop
	json2Dbl(o, curlImpl->getResField("Stop").c_str(), &tblOrder->Stop);
	// Limit
	json2Dbl(o, curlImpl->getResField("Limit").c_str(), &tblOrder->Limit);
	// Time
	json2Time(o, curlImpl->getResField("Time").c_str(), &tblOrder->Time);
	// Reserve
	json2Str(o, curlImpl->getResField("Reserve").c_str(), s);
	strcpy(tblOrder->Reserve, s.c_str());
	return tblOrder;
}

TblTrade* COrder2Rest::newTblTrade(picojson::object& o, CCurlImpl* curlImpl)
{
	TblTrade* tblTrade = new TblTrade();
	string s;
	// TradeID
	json2Str(o, curlImpl->getResField("TradeID").c_str(), s);
	strcpy(tblTrade->TradeID, s.c_str());
	// AccountID
	json2Str(o, curlImpl->getResField("AccountID").c_str(), s);
	if (s.length() == 0) {
		strcpy(tblTrade->AccountID, getBaseInfo("AccountID"));
	}
	else {
		strcpy(tblTrade->AccountID, s.c_str());
	}
	// OfferID
	json2Str(o, curlImpl->getResField("OfferID").c_str(), s);
	strcpy(tblTrade->OfferID, s.c_str());
	// Symbol
	json2Str(o, curlImpl->getResField("Symbol").c_str(), s);
	CUtils::replace(s, getSymbolInfo("Combination"), "/");
	strcpy(tblTrade->Symbol, s.c_str());
	// Amount
	json2Dbl(o, curlImpl->getResField("Amount").c_str(), &tblTrade->Amount);
	// BS
	json2Str(o, curlImpl->getResField("BS").c_str(), s);
	strcpy(tblTrade->BS, transfSide(s.c_str()).c_str());
	if (strlen(tblTrade->BS) == 0) {
		strcpy(tblTrade->BS, transfSide(tblTrade->Amount).c_str());
	}
	tblTrade->Amount = abs(tblTrade->Amount);
	// Open
	json2Dbl(o, curlImpl->getResField("Open").c_str(), &tblTrade->Open);
	// Close
	json2Dbl(o, curlImpl->getResField("Close").c_str(), &tblTrade->Close);
	// Stop
	json2Dbl(o, curlImpl->getResField("Stop").c_str(), &tblTrade->Stop);
	// Limit
	json2Dbl(o, curlImpl->getResField("Limit").c_str(), &tblTrade->Limit);
	// High
	json2Dbl(o, curlImpl->getResField("High").c_str(), &tblTrade->High);
	// Low
	json2Dbl(o, curlImpl->getResField("Low").c_str(), &tblTrade->Low);
	// PL
	if (!json2Dbl(o, curlImpl->getResField("PL").c_str(), &tblTrade->PL)) {
		tblTrade->PL = PL_INVALID;
	}
	// GrossPL
	if (!json2Dbl(o, curlImpl->getResField("GrossPL").c_str(), &tblTrade->GrossPL)) {
		tblTrade->GrossPL = PL_INVALID;
	}
	// Commission
	json2Dbl(o, curlImpl->getResField("Commission").c_str(), &tblTrade->Commission);
	// Interest
	json2Dbl(o, curlImpl->getResField("Interest").c_str(), &tblTrade->Interest);
	// OpenTime
	json2Time(o, curlImpl->getResField("OpenTime").c_str(), &tblTrade->OpenTime);
	// CloseTime
	json2Time(o, curlImpl->getResField("CloseTime").c_str(), &tblTrade->CloseTime);
	// OpenOrderID
	json2Str(o, curlImpl->getResField("OpenOrderID").c_str(), s);
	strcpy(tblTrade->OpenOrderID, s.c_str());
	// CloseOrderID
	json2Str(o, curlImpl->getResField("CloseOrderID").c_str(), s);
	strcpy(tblTrade->CloseOrderID, s.c_str());
	// StopOrderID
	json2Str(o, curlImpl->getResField("StopOrderID").c_str(), s);
	strcpy(tblTrade->StopOrderID, s.c_str());
	// LimitOrderID
	json2Str(o, curlImpl->getResField("LimitOrderID").c_str(), s);
	strcpy(tblTrade->LimitOrderID, s.c_str());
	// Reserve
	json2Str(o, curlImpl->getResField("Reserve").c_str(), s);
	strcpy(tblTrade->Reserve, s.c_str());
	return tblTrade;
}

time_t COrder2Rest::getTimetByPeriod(const char* period)
{
	for (int i = 0; period2time[i].period; i++) {
		if (strcmp(period, period2time[i].period) == 0) {
			return period2time[i].timet;
		}
	}
	return 0;
}

const char* COrder2Rest::getBaseInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("Base", key, defval);
}

const char* COrder2Rest::getHeaderInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("Header", key, defval);
}

const char* COrder2Rest::getSymbolInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("Symbol", key, defval);
}

const char* COrder2Rest::getPeriodInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("Period", key, defval);
}

const char* COrder2Rest::getSideInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("Side", key, defval);
}

const char* COrder2Rest::getErrorInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("Error", key, defval);
}

const char* COrder2Rest::getMarketInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("Market", key, defval);
}

const char* COrder2Rest::getAccountInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("GetAccount", key, defval);
}

const char* COrder2Rest::getPriceInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("GetPrice", key, defval);
}

const char* COrder2Rest::GetHistoricalDataInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("GetHistoricalData", key, defval);
}

const char* COrder2Rest::GetOpenedTradesInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("GetOpenedTrades", key, defval);
}

const char* COrder2Rest::GetClosedTradesInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("GetClosedTrades", key, defval);
}

const char* COrder2Rest::getOpenMarketOrderInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("OpenMarketOrder", key, defval);
}

const char* COrder2Rest::getStopLossOrderInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("StopLossOrder", key, defval);
}

const char* COrder2Rest::getTakeProfitOrderInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("TakeProfitOrder", key, defval);
}

const char* COrder2Rest::getChangeStopLossInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("ChangeStopLoss", key, defval);
}

const char* COrder2Rest::getChangeTakeProfitInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("ChangeTakeProfit", key, defval);
}

const char* COrder2Rest::getCloseTradeInfo(const char* key, const char* defval)
{
	return m_SimpleIni.GetValue("CloseTrade", key, defval);
}
