#pragma once 
#include "Netplay.h"
#include "IopCommon.h"
#include "App.h"

extern bool					g_NetEnabled;
extern ScopedPtr<Netplay>	g_NetCore;

struct EmulatorSyncState
{
	EmulatorSyncState() : cdvdCRC(0) { memset(biosVersion, 0, 15); }
	char biosVersion[20];
	u32 cdvdCRC;
	u64 mcd1CRC;
	u64 mcd2CRC;
	u8 delay;
	bool operator==(const EmulatorSyncState& other);
	bool operator!=(const EmulatorSyncState& other);
};

namespace Net
{
	EmulatorSyncState getSyncState();
	void Open();
	void Init();
	void Close();
	void ReplaceHandlers();
	void RestoreHandlers();
	
	//let's test it...
	extern _PADupdate		  PADupdateBackup;

	extern _PADopen           PADopenBackup;
	extern _PADstartPoll      PADstartPollBackup;
	extern _PADpoll           PADpollBackup;
	extern _PADquery          PADqueryBackup;
	extern _PADkeyEvent       PADkeyEventBackup;
	extern _PADsetSlot        PADsetSlotBackup;
	extern _PADqueryMtap      PADqueryMtapBackup;

	void CALLBACK NETPADupdate(int pad);

	s32 CALLBACK NETPADopen(void *pDsp);
	u8 CALLBACK NETPADstartPoll(int pad);
	u8 CALLBACK NETPADpoll(u8 value);
	u32 CALLBACK NETPADquery(int pad);
	keyEvent* CALLBACK NETPADkeyEvent();
	s32 CALLBACK NETPADsetSlot(u8 port, u8 slot);
	s32 CALLBACK NETPADqueryMtap(u8 port);
}
