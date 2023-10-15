#ifndef IBASEORDER_H
#define IBASEORDER_H

#include "Table.h"

#define RET_SUCCESS	0
#define RET_FAILED	-1

class IBaseOrder
{
public:
	virtual int init(const char* iniFile) = 0;
	virtual int login() = 0;
	virtual int close() = 0;
	virtual time_t getServerTime() = 0;
	virtual int getAccount(const char* accountID, TblAccount** tblAccount) = 0;
	virtual int getPrice(const char* symbols[], TblPrice** pTblPrice[]) = 0;
	virtual int getOpenedTrades(TblTrade** pTblTrade[]) = 0;
	virtual int getClosedTrades(TblTrade** pTblTrade[]) = 0;
	virtual int getHistoricalData(const char* symbol, const char* period, time_t start, time_t end, bool maxRange, TblCandle** pTblCandle[]) = 0;
	virtual int openMarketOrder(TblOrder* tblOrder) = 0;
	virtual int changeStopLoss(TblTrade* tblTrade) = 0;
	virtual int changeTakeProfit(TblTrade* tblTrade) = 0;
	virtual int closeTrade(TblTrade* tblTrade) = 0;
};

#endif
