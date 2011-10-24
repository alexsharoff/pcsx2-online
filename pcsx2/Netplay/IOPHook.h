#pragma once
#include "App.h"
#include "IopCommon.h"

class IOPHook
{
public:
	virtual u8 HandleIO(int side, int index, u8 value);
	virtual void NextFrame();
	virtual void AcceptInput(int side);
};


IOPHook* g_IOPHook = 0;

namespace
{
	_PADupdate		   PADupdateBackup;
	_PADopen           PADopenBackup;
	_PADstartPoll      PADstartPollBackup;
	_PADpoll           PADpollBackup;
	_PADquery          PADqueryBackup;
	_PADkeyEvent       PADkeyEventBackup;
	_PADsetSlot        PADsetSlotBackup;
	_PADqueryMtap      PADqueryMtapBackup;
	
	s64 g_frameId;
	int g_cmd42Counter;
	int g_pollSide;
	int g_pollIndex;

	s32 CALLBACK NETPADopen(void *pDsp)
	{
		return PADopenBackup(pDsp);
	}
	u8 CALLBACK NETPADstartPoll(int pad)
	{
		return PADstartPollBackup(pad);
	}
	u32 CALLBACK NETPADquery(int pad)
	{
		return PADqueryBackup(pad);
	}
	keyEvent* CALLBACK NETPADkeyEvent()
	{
		return PADkeyEventBackup();
	}
	s32 CALLBACK NETPADqueryMtap(u8 port)
	{
		return PADqueryMtapBackup(port);
	}
	void CALLBACK NETPADupdate(int pad)
	{
		return PADupdateBackup(pad);
	}

	u8 CALLBACK NETPADpoll(u8 value)
	{
		if(g_pollIndex == 0 && value == 0x42 && g_pollSide == 0)
			g_cmd42Counter++;

		value = PADpollBackup(value);

		if(g_cmd42Counter > 2 && g_pollIndex > 1)
		{
			if(g_pollIndex <= 7)
			{
				if(g_IOPHook)
					value = g_IOPHook->HandleIO(g_pollSide, g_pollIndex-2,value);

				if(g_IOPHook && g_pollIndex == 7)
					g_IOPHook->AcceptInput(g_pollSide);
			}
			else
				value = 0xff;
		}
		g_pollIndex++;
		return value;
	}
	s32 CALLBACK NETPADsetSlot(u8 port, u8 slot)
	{
		g_pollSide = port - 1;
		g_pollIndex = 0;

		if(g_pollSide == 0)
		{
			++g_frameId;
			if(g_IOPHook && g_cmd42Counter > 2)
				g_IOPHook->NextFrame();
		}
		return PADsetSlotBackup(port, slot);
	}
}


void HookIOP()
{
	g_frameId = -1;
	g_cmd42Counter = 0;
	g_pollSide = 0;
	g_pollIndex = 0;

	PADopenBackup = PADopen;
	PADstartPollBackup = PADstartPoll;
	PADpollBackup = PADpoll;
	PADqueryBackup = PADquery;
	PADkeyEventBackup = PADkeyEvent;
	PADsetSlotBackup = PADsetSlot;
	PADqueryMtapBackup = PADqueryMtap;
	PADupdateBackup = PADupdate;
		
	PADopen = NETPADopen;
	PADstartPoll = NETPADstartPoll;
	PADpoll = NETPADpoll;
	PADquery = NETPADquery;
	PADkeyEvent = NETPADkeyEvent;
	PADsetSlot = NETPADsetSlot;
	PADqueryMtap = NETPADqueryMtap;
	PADupdate = NETPADupdate;
}

void UnhookIOP()
{
	PADopen = PADopenBackup;
	PADstartPoll = PADstartPollBackup;
	PADpoll = PADpollBackup;
	PADquery = PADqueryBackup;
	PADkeyEvent = PADkeyEventBackup;
	PADsetSlot = PADsetSlotBackup;
	PADqueryMtap = PADqueryMtapBackup;
	PADupdate = PADupdateBackup;
}