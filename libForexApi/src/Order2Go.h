#ifndef ORDER2GO_H
#define ORDER2GO_H

#include "ForexConnect/ForexConnect.h"
#include "IBaseOrder.h"
#include "SessionStatusListener.h"
#include "ResponseListener.h"
#include "TableListener.h"

class COrder2Go : public IBaseOrder
{
private:
	IO2GSession *m_pSession;
	CSessionStatusListener *m_pSessionStatusListener;
	CResponseListener *m_pResponseListener;
	CTableListener *m_pTableListener;
	bool m_bSubscribed;
	CSimpleIniCaseA m_SimpleIni;
	IPluginProxy *m_pPluginProxy;

public:
	COrder2Go();

	int init(const char* iniFile);
	int login();
	int close();
	time_t getServerTime();
	int getAccount(const char* accountID, TblAccount** tblAccount);
	int getPrice(const char* symbols[], TblPrice** pTblPrice[]);
	int getOpenedTrades(TblTrade** pTblTrade[]);
	int getClosedTrades(TblTrade** pTblTrade[]);
	int getHistoricalData(const char* symbol, const char* period, time_t start, time_t end, bool maxRange, TblCandle** pTblCandle[]);
	int openMarketOrder(TblOrder* tblOrder);
	int changeStopLoss(TblTrade* tblTrade);
	int changeTakeProfit(TblTrade* tblTrade);
	int closeTrade(TblTrade* tblTrade);

private:
	int logout();
	void onConnected();
	void logTimeFrames();
	void printSettings();
	void onSystemPropertiesReceived(IO2GResponse *response);
	int subscribeTableListener(IO2GTableManager *manager, IO2GTableListener *listener);
	void unsubscribeTableListener(IO2GTableManager *manager, IO2GTableListener *listener);
	int getHistoricalData(const char* symbol, const char* period, time_t start, int maxNumber, vector<TblCandle*>& tblCandleList);
	int getHistoricalData(const char* symbol, const char* period, time_t start, time_t end, int maxNumber, vector<TblCandle*>& tblCandleList);
	int getHistoricalData(const char* symbol, const char* period, double start, double end, int maxNumber, vector<TblCandle*>& tblCandleList);
	int candleOfReader(const char* symbol, const char* period, IO2GMarketDataSnapshotResponseReader* reader, vector<TblCandle*>& tblCandleList);
	string getOrderID(string tradeID, string orderType);
	string getLimitOrderID(string tradeID);
	string getStopOrderID(string tradeID);
	const char* getLoginInfo(const char* key, const char* defval = "");
	const char* getMarketInfo(const char* key, const char* defval = "");
	static time_t getTimetByPeriod(const char* period);
};

#endif
