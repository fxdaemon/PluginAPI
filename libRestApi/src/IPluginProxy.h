#ifndef IPLUGINPROXY_H
#define IPLUGINPROXY_H

#include "IBaseOrder.h"

typedef enum {
	MSG_ERROR = 0,
	MSG_WARN = 1,
	MSG_INFO = 2,
	MSG_DEBUG = 3,
} MsgLevel;

class IPluginProxy
{
protected:
	IPluginProxy();
public:
	virtual bool registerPlugin(const char* name, IBaseOrder* baseOrder) = 0;
	virtual void onDisconnected() = 0;
	virtual void onMessage(MsgLevel level, const char* message) = 0;
	virtual void onPrice(TableStatus status, const TblPrice* tblPrice) = 0;
	virtual void onAccount(TableStatus status, const TblAccount* tblAccount) = 0;
	virtual void onOrder(TableStatus status, const TblOrder* tblOrder) = 0;
	virtual void onOpenedTrade(TableStatus status, const TblTrade* tblTrade) = 0;
	virtual void onClosedTrade(TableStatus status, const TblTrade* tblTrade) = 0;
};

extern "C" {
#ifdef WIN32
	#define EXPORT __declspec(dllexport)
	#define IMPORT __declspec(dllimport)
	#ifdef PLUGIN_EXPORTS
		EXPORT IPluginProxy* getPluginProxy();
	#else
		IMPORT IPluginProxy* getPluginProxy();
	#endif
#else
	IPluginProxy* getPluginProxy();
#endif
}

#endif
