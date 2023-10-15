#ifndef TABLE_H
#define TABLE_H

#include <time.h>

typedef enum {
	ST_UNKNOWN = -1,
	ST_NEW = 0,
	ST_UPD = 1,
	ST_DEL = 2
} TableStatus;

struct TblPrice
{
	char OfferID[20];
	char Symbol[40];
	char SymbolType[20];
	double Bid;
	double Ask;
	double High;
	double Low;
	double PipCost;
	double PointSize;
	time_t Time;
	char Reserve[120];
};

struct TblCandle
{
	char Symbol[40];
	time_t StartDate;
	char Period[20];
	double AskClose;
	double AskHigh;
	double AskLow;
	double AskOpen;
	double BidClose;
	double BidHigh;
	double BidLow;
	double BidOpen;
};

struct TblAccount
{
	char AccountID[40];
	char AccountName[120];
	char AccountType[20];
	double Balance;
	double Equity;
	double DayPL;
	double GrossPL;
	double UsedMargin;
	double UsableMargin;
	double UsableMarginInPercent;
	double UsableMaintMarginInPercent;
	double MarginRate;
	char Hedging[10];
	char Currency[10];
	char Broker[20];
	char Reserve[120];
};

struct TblOrder
{
	char OrderID[40];
	char RequestID[120];
	char AccountID[40];
	char OfferID[20];
	char Symbol[40];
	char TradeID[40];
	char BS[10];
	char Stage[20];
	char OrderType[20];
	char OrderStatus[20];
	double Amount;
	double Rate;
	double Stop;
	double Limit;
	time_t Time;
	char Reserve[120];
};

struct TblTrade
{
	char TradeID[40];
	char AccountID[40];
	char OfferID[20];
	char Symbol[40];
	double Amount;
	char BS[10];
	double Open;
	double Close;
	double Stop;
	double Limit;
	double High;
	double Low;
	double PL;
	double GrossPL;
	double Commission;
	double Interest;
	time_t OpenTime;
	time_t CloseTime;
	char OpenOrderID[40];
	char CloseOrderID[40];
	char StopOrderID[40];
	char LimitOrderID[40];
	char Reserve[120];
};

#endif
