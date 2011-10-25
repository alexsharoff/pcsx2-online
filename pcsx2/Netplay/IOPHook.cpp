#include "PrecompiledHeader.h"
#include "IOPHook.h"

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
	
	s64 g_frameId = -1;
	int g_cmd42Counter = -1;
	int g_pollSide = -1;
	int g_pollIndex = -1;
	bool g_active = false;

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

void HookIOP(IOPHook* hook)
{
	g_IOPHook = hook;
	g_frameId = -1;
	g_cmd42Counter = 0;
	g_pollSide = 0;
	g_pollIndex = 0;

	if(g_active)
		return;
	g_active = true;

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
	g_IOPHook = 0;
	PADopen = PADopenBackup;
	PADstartPoll = PADstartPollBackup;
	PADpoll = PADpollBackup;
	PADquery = PADqueryBackup;
	PADkeyEvent = PADkeyEventBackup;
	PADsetSlot = PADsetSlotBackup;
	PADqueryMtap = PADqueryMtapBackup;
	PADupdate = PADupdateBackup;
	g_active = false;
}