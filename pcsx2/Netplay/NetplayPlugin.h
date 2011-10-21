#pragma once 

#include "IopCommon.h"
#include "App.h"

struct ConnectionEstimate
{
	int delay;
};


class INetplayPlugin
{
protected:
	static INetplayPlugin* instance;
public:
	static INetplayPlugin& GetInstance();

	virtual void Enable(bool enabled) = 0;
	virtual bool IsEnabled() = 0;

	virtual void Open() = 0;
	virtual void Init() = 0;
	virtual void Close() = 0;

	virtual s32 CALLBACK NETPADopen(void *pDsp) = 0;
	virtual u8 CALLBACK NETPADstartPoll(int pad) = 0;
	virtual u8 CALLBACK NETPADpoll(u8 value) = 0;
	virtual u32 CALLBACK NETPADquery(int pad) = 0;
	virtual keyEvent* CALLBACK NETPADkeyEvent() = 0;
	virtual s32 CALLBACK NETPADsetSlot(u8 port, u8 slot) = 0;
	virtual s32 CALLBACK NETPADqueryMtap(u8 port) = 0;
	virtual void CALLBACK NETPADupdate(int pad) = 0;
};
