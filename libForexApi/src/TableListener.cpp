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
#include "TableListener.h"

void CTableListener::onStatusChanged(O2GTableStatus status)
{
}

void CTableListener::onAdded(const char* rowID, IO2GRow* row)
{
	onTableRowAdded(TableStatus::ST_NEW, row);
}

void CTableListener::onChanged(const char* rowID, IO2GRow* row)
{
	onTableRowAdded(TableStatus::ST_UPD, row);
}

void CTableListener::onDeleted(const char* rowID, IO2GRow* row)
{
	onTableRowAdded(TableStatus::ST_DEL, row);
}

void CTableListener::onTableRowAdded(TableStatus status, IO2GRow* row)
{
	switch (row->getTableType()) {
	case Offers:
		{
			TblPrice* tblPrice = makTblPrice((IO2GOfferTableRow*)row);
			m_pPluginProxy->onPrice(status, tblPrice);
			delete tblPrice;
		}
		break;
	case Accounts:
		{
			TblAccount* tblAccount = makTblAccount((IO2GAccountTableRow*)row);
			m_pPluginProxy->onAccount(status, tblAccount);
			delete tblAccount;
		}
		break;
	case Orders:
		{
			TblOrder* tblOrder = makTblOrder((IO2GOrderTableRow*)row);
			m_pPluginProxy->onOrder(status, tblOrder);
			delete tblOrder;
		}
		break;
	case Trades:
		{
			TblTrade* tblTrade = makOpenTblTrade((IO2GTradeTableRow*)row);
			m_pPluginProxy->onOpenedTrade(status, tblTrade);
			delete tblTrade;
		}
		break;
	case ClosedTrades:
		{
			TblTrade* tblTrade = makClosedTblTrade((IO2GClosedTradeTableRow*)row);
			if (strlen(tblTrade->CloseOrderID) > 0) {
				m_pPluginProxy->onClosedTrade(status, tblTrade);
			}
			delete tblTrade;
		}
		break;
	default:
		break;
	}
}

time_t CTableListener::date2Time(DATE date)
{
	tm cal;
	if (CO2GDateUtils::OleTimeToCTime(date, &cal)) {
		return CUtils::getUTCTime(&cal);
	} else {
		return 0;
	}
}

DATE CTableListener::time2Date(tm t)
{
	DATE dt = 0;
	CO2GDateUtils::CTimeToOleTime(&t, &dt);
	return dt;
}

TblAccount* CTableListener::makTblAccount(IO2GAccountRow *accountRow)
{
	TblAccount* tblAccount = new TblAccount();
	strcpy(tblAccount->AccountID, accountRow->getAccountID());
	strcpy(tblAccount->AccountName, accountRow->getAccountName());
	strcpy(tblAccount->AccountType, accountRow->getAccountKind());
	tblAccount->Balance = accountRow->getBalance();
//	tblAccount->UsedMargin = accountRow->getHadgeMarginPCT();
	tblAccount->MarginRate = 0;
	strcpy(tblAccount->Hedging, accountRow->getMaintenanceType());
	tblAccount->Currency[0] = '\0';
	strcpy(tblAccount->Broker, "fxcm");
	tblAccount->Reserve[0] = '\0';
	return tblAccount;
}

TblAccount* CTableListener::makTblAccount(IO2GAccountTableRow *accountRow)
{
	TblAccount* tblAccount = makTblAccount((IO2GAccountRow*)accountRow);
	tblAccount->UsedMargin = accountRow->getUsedMargin();
	tblAccount->Equity = accountRow->getEquity();
	tblAccount->DayPL = accountRow->getDayPL();
	tblAccount->UsableMargin = accountRow->getUsableMargin();
	tblAccount->GrossPL = accountRow->getGrossPL();
	tblAccount->UsableMarginInPercent = accountRow->getUsableMarginInPercentage();
	tblAccount->UsableMaintMarginInPercent = accountRow->getUsableMaintMarginInPercentage();
	return tblAccount;
}

TblPrice* CTableListener::makTblPrice(IO2GOfferRow *offerRow)
{
	TblPrice* tblPrice = new TblPrice();
	strcpy(tblPrice->OfferID, offerRow->getOfferID());
	strcpy(tblPrice->Symbol, offerRow->getInstrument());
	strcpy(tblPrice->SymbolType, std::to_string(offerRow->getInstrumentType()).c_str());
	tblPrice->Bid = offerRow->getBid();
	tblPrice->Ask = offerRow->getAsk();
	tblPrice->High = offerRow->getHigh();
	tblPrice->Low = offerRow->getLow();
	tblPrice->Time = date2Time(offerRow->getTime());
	if (offerRow->getPointSize() > 0) {
		tblPrice->PointSize = offerRow->getPointSize();
	}
	tblPrice->Reserve[0] = '\0';
	return tblPrice;
}

TblPrice* CTableListener::makTblPrice(IO2GOfferTableRow *offerRow)
{
	TblPrice* tblPrice = makTblPrice((IO2GOfferRow*)offerRow);
	tblPrice->PipCost = offerRow->getPipCost();
	return tblPrice;
}

TblOrder* CTableListener::makTblOrder(IO2GOrderRow *orderRow)
{
	TblOrder* tblOrder = new TblOrder();
	strcpy(tblOrder->OrderID, orderRow->getOrderID());
	strcpy(tblOrder->RequestID, orderRow->getRequestID());
	strcpy(tblOrder->AccountID, orderRow->getAccountID());
	strcpy(tblOrder->OfferID, orderRow->getOfferID());
	strcpy(tblOrder->TradeID, orderRow->getTradeID());
	strcpy(tblOrder->BS, orderRow->getBuySell());
	strcpy(tblOrder->Stage, orderRow->getStage());
	strcpy(tblOrder->OrderType, orderRow->getType());
	strcpy(tblOrder->OrderStatus, orderRow->getStatus());
	tblOrder->Amount = (double)orderRow->getAmount();
	tblOrder->Rate = orderRow->getRate();
	tblOrder->Time = date2Time(orderRow->getStatusTime());
	tblOrder->Reserve[0] = '\0';
	return tblOrder;
}

TblOrder* CTableListener::makTblOrder(IO2GOrderTableRow *orderRow)
{
	TblOrder* tblOrder = makTblOrder((IO2GOrderRow*)orderRow);
	tblOrder->Stop = orderRow->getStop();
	tblOrder->Limit = orderRow->getLimit();
	return tblOrder;
}

TblTrade* CTableListener::makOpenTblTrade(IO2GTradeRow *tradeRow)
{
	TblTrade* tblTrade = new TblTrade();
	strcpy(tblTrade->TradeID, tradeRow->getTradeID());
	strcpy(tblTrade->AccountID, tradeRow->getAccountID());
	strcpy(tblTrade->OfferID, tradeRow->getOfferID());
	tblTrade->Amount = tradeRow->getAmount();
	strcpy(tblTrade->BS, tradeRow->getBuySell());
	tblTrade->Open = tradeRow->getOpenRate();
	tblTrade->Commission = tradeRow->getCommission();
	tblTrade->Interest = tradeRow->getRolloverInterest();
	tblTrade->OpenTime = date2Time(tradeRow->getOpenTime());
	tblTrade->CloseTime = 0;
	strcpy(tblTrade->OpenOrderID, tradeRow->getOpenOrderID());
	tblTrade->CloseOrderID[0] = '\0';
	tblTrade->StopOrderID[0] = '\0';
	tblTrade->LimitOrderID[0] = '\0';
	tblTrade->Reserve[0] = '\0';
	return tblTrade;
}

TblTrade* CTableListener::makOpenTblTrade(IO2GTradeTableRow *tradeRow)
{
	TblTrade* tblTrade = makOpenTblTrade((IO2GTradeRow*)tradeRow);
	tblTrade->Close = tradeRow->getClose();
	tblTrade->Stop = tradeRow->getStop();
	tblTrade->Limit = tradeRow->getLimit();
	tblTrade->High = tradeRow->getClose();
	tblTrade->Low = tradeRow->getClose();
	tblTrade->PL = tradeRow->getPL();
	tblTrade->GrossPL = tradeRow->getGrossPL();
	return tblTrade;
}

TblTrade* CTableListener::makClosedTblTrade(IO2GClosedTradeRow *tradeRow)
{
	TblTrade* tblTrade = new TblTrade();
	strcpy(tblTrade->TradeID, tradeRow->getTradeID());
	strcpy(tblTrade->AccountID, tradeRow->getAccountID());
	strcpy(tblTrade->OfferID, tradeRow->getOfferID());
	tblTrade->Amount = tradeRow->getAmount();
	strcpy(tblTrade->BS, tradeRow->getBuySell());
	tblTrade->Open = tradeRow->getOpenRate();
	tblTrade->Close = tradeRow->getCloseRate();
	tblTrade->Stop = 0;
	tblTrade->Limit = 0;
	tblTrade->PL = PL_INVALID;
	tblTrade->GrossPL = tradeRow->getGrossPL();
	tblTrade->Commission = tradeRow->getCommission();
	tblTrade->Interest = tradeRow->getRolloverInterest();
	tblTrade->OpenTime = date2Time(tradeRow->getOpenTime());
	tblTrade->CloseTime = date2Time(tradeRow->getCloseTime());
	strcpy(tblTrade->OpenOrderID, tradeRow->getOpenOrderID());
	strcpy(tblTrade->CloseOrderID, tradeRow->getCloseOrderID());
	tblTrade->StopOrderID[0] = '\0';
	tblTrade->LimitOrderID[0] = '\0';
	tblTrade->Reserve[0] = '\0';
	return tblTrade;
}

TblTrade* CTableListener::makClosedTblTrade(IO2GClosedTradeTableRow *tradeRow)
{
	TblTrade *tblTrade = makClosedTblTrade((IO2GClosedTradeRow*)tradeRow);
	tblTrade->PL = tradeRow->getPL();
	tblTrade->GrossPL = tradeRow->getGrossPL();
	return tblTrade;
}
