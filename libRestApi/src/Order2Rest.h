#ifndef ORDER2REST_H
#define ORDER2REST_H

#include "IBaseOrder.h"
#include "IPluginProxy.h"

#define PL_INVALID DBL_MAX

typedef enum {
	CURL_GET_PRICE,
	CURL_GET_ACCOUNT,
	CURL_GET_OPENTRADES,
	CURL_GET_CLOSEDTRADES,
	CURL_GET_CANDLES
};

class COrder2Rest : public IBaseOrder
{
private:
	CSimpleIniCaseA m_SimpleIni;
	IPluginProxy *m_pPluginProxy;
	CURLM *m_pCurlMulti;
	CCurlImpl* m_CurlList[5];
	map<string, string> m_mapPathParams;
	HANDLE m_hExitEvent;
	HANDLE m_hOverEvent;
	CThread *m_pTradeEventsProcessThread;
	atomic<time_t> m_tmStdTime;

public:
	COrder2Rest();
	~COrder2Rest();

	int init(const char* iniFile);
	int login();
	int close();
	time_t getServerTime();
	int getAccount(const char* accountID, TblAccount** tblAccount);
	int getPrice(const char* symbol[], TblPrice** pTblPrice[]);
	int getOpenedTrades(TblTrade** pTblTrade[]);
	int getClosedTrades(TblTrade** pTblTrade[]);
	int getHistoricalData(const char* symbol, const char* period, time_t start, time_t end, bool maxRange, TblCandle** pTblCandle[]);
	int openMarketOrder(TblOrder* tblOrder);
	int openStopLossOrder(TblOrder* tblOrder);
	int openTakeProfitOrder(TblOrder* tblOrder);
	int changeStopLoss(TblTrade* tblTrade);
	int changeTakeProfit(TblTrade* tblTrade);
	int closeTrade(TblTrade* tblTrade);

private:
	int initCurl();
	int startTradeEventThread();	
	static void tradeEventsProcess(void *pv);
	void waitNextEvent();
	void onTableListener();
	static void onGetPrice(void* curlobj, void* listener);
	static void onGetAccount(void* curlobj, void* listener);
	static void onGetOpenTrades(void* curlobj, void* listener);
	static void onGetClosedTrades(void* curlobj, void* listener);

	int onJsonError(picojson::object& o);
	picojson::object& parseJson(CCurlImpl* curlImpl, picojson::value& json);
	picojson::object& parseJsonObject(CCurlImpl* curlImpl, picojson::value& json);
	picojson::array& parseJsonArray(CCurlImpl* curlImpl, picojson::value& json);
	time_t reqServerTime();
	int getHistoricalData(const char* symbol, const char* period, time_t start, time_t end, int adjustmentTimezone, vector<TblCandle*>& tblCandleList);

	bool getSslVerify();
	void getHeaders(vector<const char*>& headers);
	int getAdjustmentTimezone();	// 2023/10/09 add by yld
	string transfSymbol(const char* symbol);
	string transfSymbols(const char* symbols[]);
	string transfPeriod(const char* period);
	string transfSide(const char* side);
	string transfSide(double amount);
	string transfTime(time_t t);
	
	TblCandle* newTblCandle(picojson::object& o, CCurlImpl* curlImpl);
	TblAccount* newTblAccount(picojson::object& o, CCurlImpl* curlImpl);
	TblPrice* newTblPrice(picojson::object& o, CCurlImpl* curlImpl);
	TblOrder* newTblOrder(picojson::object& o, CCurlImpl* curlImpl);
	TblTrade* newTblTrade(picojson::object& o, CCurlImpl* curlImpl);
	
	static picojson::value& findJsonValue(picojson::object& o, const char* key);
	static bool json2Bool(picojson::object& o, const char* key, bool* val);
	static bool json2Int(picojson::object& o, const char* key, int* val);
	static bool json2Lng(picojson::object& o, const char* key, long* val);
	static bool json2Dbl(picojson::object& o, const char* key, double* val);
	static bool json2Str(picojson::object& o, const char* key, string& val);
	static bool json2Time(picojson::object& o, const char* key, time_t* val);
	static time_t getTimetByPeriod(const char* period);

	const char* getBaseInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getHeaderInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getSymbolInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getPeriodInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getSideInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getErrorInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getMarketInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getAccountInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getPriceInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* GetHistoricalDataInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* GetOpenedTradesInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* GetClosedTradesInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getOpenMarketOrderInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getStopLossOrderInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getTakeProfitOrderInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getChangeStopLossInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getChangeTakeProfitInfo(const char* key, const char* defval = CCurlImpl::Blank);
	const char* getCloseTradeInfo(const char* key, const char* defval = CCurlImpl::Blank);
};

#endif
