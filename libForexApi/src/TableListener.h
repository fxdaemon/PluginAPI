#ifndef TABLELISTENER_H
#define TABLELISTENER_H

#define PL_INVALID DBL_MAX

#include "ForexConnect/ForexConnect.h"
#include "CriticalSection.h"
#include "IPluginProxy.h"
#include "Table.h"

class CTableListener : public IO2GTableListener
{
private:
	IPluginProxy *m_pPluginProxy;

public:
	CTableListener(IPluginProxy* pluginProxy)
		: m_pPluginProxy(pluginProxy) {};

	long addRef() { return 0; };
	long release() { return 0; };
	void onStatusChanged(O2GTableStatus);
	void onAdded(const char* rowID, IO2GRow* row);
	void onChanged(const char* rowID,IO2GRow* row);
	void onDeleted(const char* rowID,IO2GRow* row);

	static time_t date2Time(DATE date);
	static DATE time2Date(tm t);
	static TblAccount* makTblAccount(IO2GAccountRow* accountRow);
	static TblAccount* makTblAccount(IO2GAccountTableRow* accountRow);
	static TblPrice* makTblPrice(IO2GOfferRow* offerRow);
	static TblPrice* makTblPrice(IO2GOfferTableRow* offerRow);
	static TblOrder* makTblOrder(IO2GOrderRow* orderRow);
	static TblOrder* makTblOrder(IO2GOrderTableRow* orderRow);
	static TblTrade* makOpenTblTrade(IO2GTradeRow* tradeRow);
	static TblTrade* makOpenTblTrade(IO2GTradeTableRow* tradeRow);
	static TblTrade* makClosedTblTrade(IO2GClosedTradeRow* tradeRow);
	static TblTrade* makClosedTblTrade(IO2GClosedTradeTableRow* tradeRow);

private:
	void onTableRowAdded(TableStatus status, IO2GRow* row);
};

#endif