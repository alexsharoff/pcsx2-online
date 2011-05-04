#include "PrecompiledHeader.h"
#include "Net.h"
#include "AppConfig.h"
#include <iostream>
#include <fstream>
#include "ps2/BiosTools.h"
#include "Elfheader.h"


ScopedPtr<Netplay>	g_NetCore;
bool				g_NetEnabled = false;


bool EmulatorSyncState::operator==(const EmulatorSyncState& other)
{
	return (memcmp(biosVersion, other.biosVersion, sizeof(biosVersion)) == 0
		&& cdvdCRC == other.cdvdCRC && mcd1CRC == other.mcd1CRC 
		&& mcd2CRC == other.mcd2CRC);
}
bool EmulatorSyncState::operator!=(const EmulatorSyncState& other)
{
	return !(*this == other);
}


struct padPoller {
public:
	padPoller() {
		pollCounter = 0;
		pressureMode = false;
		side = 1;
		delay = 0;
		frame = 0;
		cmd42Counter = 0;
		lastSent = -1;
		frameIsSkipped = false;
		error = false;
	}
	int pollCounter;
	bool pressureMode;
	int side;
	int delay;
	int frame;
	int cmd42Counter;
	bool delayBufferReady;
	int lastSent;
	bool active;
	int mySide;
	bool frameIsSkipped;
	std::string errorMsg;
	bool error;
	wxString info;
} poller;


u8 defaultInput[] = {0xff, 0xff, 0x7f, 0x7f, 0x7f, 0x7f};
bool localInputIsSet(int id)
{
	return g_NetCore->getInput(id, poller.side, -1) != 0;
}
bool remoteInputIsSet(int id)
{
	return g_NetCore->getInput(id, 1-poller.side, -1) != 0;
}

namespace Net
{
	Pcsx2Config EmuOptionsBackup;
	bool Mcd1EnabledBackup;
	bool Mcd2EnabledBackup;
	bool EnableGameFixesBackup;
	AppConfig::ConsoleLogOptions ProgLogBoxBackup;

	EmulatorSyncState getSyncState()
	{
		EmulatorSyncState syncState;

		cdvdReloadElfInfo();
		syncState.cdvdCRC = ElfCRC;

		wxString biosDesc;
		IsBIOS(g_Conf->EmuOptions.BiosFilename.GetFullPath(), biosDesc);

		memcpy(syncState.biosVersion, biosDesc.ToAscii().data(), 
			biosDesc.length() > sizeof(syncState.biosVersion) ? 
			sizeof(syncState.biosVersion) : biosDesc.length());

		syncState.mcd1CRC = SysPlugins.McdGetCRC( 0, 0 );
		syncState.mcd2CRC = SysPlugins.McdGetCRC( 1, 0 );
		return syncState;
	}
	std::fstream log;
	bool replaced;
	bool first;
	Frame* myFrame;

	_PADupdate		   PADupdateBackup;
	_PADopen           PADopenBackup;
	_PADstartPoll      PADstartPollBackup;
	_PADpoll           PADpollBackup;
	_PADquery          PADqueryBackup;
	_PADkeyEvent       PADkeyEventBackup;
	_PADsetSlot        PADsetSlotBackup;
	_PADqueryMtap      PADqueryMtapBackup;

	using namespace std;
	s32 CALLBACK NETPADopen(void *pDsp)
	{
		return PADopenBackup(pDsp);
	}
	u8 CALLBACK NETPADstartPoll(int pad)
	{
		u8 r = PADstartPollBackup(pad);
		log << "p" << pad << ':';
		return r;
	}
	u8 CALLBACK NETPADpoll(u8 value)
	{
		if(poller.pollCounter == 0 && value == 0x42 && poller.side == 0)
			poller.cmd42Counter++;

		u8 r = PADpollBackup(value);

		if(!poller.active)
		{
			RestoreHandlers();
			CoreThread.Reset();
			Console.Error(poller.errorMsg.c_str());
			return r;
		}

		if(poller.cmd42Counter > 2 && poller.pollCounter > 1 && !poller.frameIsSkipped)
		{
			if(poller.info.length())
			{
				Console.WriteLn(Color_StrongGreen, poller.info.c_str());
				poller.info.clear();
			}
			if(poller.pollCounter <=7)
			{
				if(poller.side == 0)
				{
					myFrame->getData()[poller.pollCounter-2] = r;
					if(poller.pollCounter == 7)
					{
						g_NetCore->setInput(myFrame, myFrame->id(), poller.mySide);
						g_NetCore->sendInput(g_NetCore->getPeers()[0]->remote_ep, poller.mySide);
					}
				}
				int tryCount = 10;
				bool success = false;
				while(tryCount--)
				{
					Frame* f = g_NetCore->getInput(poller.frame, poller.side, 300);
					if(f != 0)
					{
						r = f->getData()[poller.pollCounter-2];
						success = true;
						break;
					}
					else
						g_NetCore->sendInput(g_NetCore->getPeers()[0]->remote_ep, poller.mySide);
				}
				if(!success)
				{
					if(getticks() - g_NetCore->lastResponce() > 5000)
					{
						int d = getticks() - g_NetCore->lastResponce();
						RestoreHandlers();
						CoreThread.Reset();
						Console.Error( "No responce from remote endpoint (%d). Closing.",d );
					}
					else
					{
						poller.frameIsSkipped = true;
						Console.Error( "Skipped frame #%d-%d. Desync may occur.", poller.frame, (poller.pollCounter-2) );
					}
				}
			}
			else
				r = 0xff;
		}
		log << " " << hex << (int)value << ">" << hex << (int)r;
		poller.pollCounter++;
		return r;
	}
	u32 CALLBACK NETPADquery(int pad)
	{
		return PADqueryBackup(pad);
	}
	keyEvent* CALLBACK NETPADkeyEvent()
	{
		return PADkeyEventBackup();
	}
	s32 CALLBACK NETPADsetSlot(u8 port, u8 slot)
	{
		if(!poller.active)
		{
			RestoreHandlers();
			CoreThread.Reset();
			Console.Error(poller.errorMsg.c_str());
		}
		else
		{

			if(!first)
			{
				if(port == 2)
					log << endl;
				if(port == 1)
				{
					log << endl << endl;
					if(poller.cmd42Counter > 2)
						poller.frame++;
				}
			}
			poller.frameIsSkipped = false;
			poller.pollCounter = 0;
			poller.side = port-1;
			if(poller.cmd42Counter >=2 && poller.side == 0)
				myFrame = new Frame(poller.frame+poller.delay, new char[7], 7);
			first = false;

			log << "(p" << (int)port << " s" << (int)slot << ") ";
		}
		return PADsetSlotBackup(port, slot);
	}
	s32 CALLBACK NETPADqueryMtap(u8 port)
	{
		return PADqueryMtapBackup(port);
	}
	void CALLBACK NETPADupdate(int pad)
	{
		if(PADupdateBackup)
			PADupdateBackup(pad);
	}
	void Open()
	{
		srand(1);
		g_NetEnabled = true;

		poller = padPoller();
		poller.delay = g_NetCore->getPeers()[0]->stats.rtt()/32 + 1;
		poller.active = true;
		poller.mySide = g_Conf->Net.Connect ? 1:0;

		replaced = false;

		char fname[256];
		sprintf(fname, "ios_log_%d.txt", time(0));
		log.open(fname, ios_base::out + ios_base::trunc);
		ReplaceHandlers();
		first = true;

		for(int i = 0; i < poller.delay; i++)
		{
			g_NetCore->setInput(new Frame(i , (char*)defaultInput, 7), i, 0);
			g_NetCore->setInput(new Frame(i , (char*)defaultInput, 7), i, 1);
		}

		EmulatorSyncState syncState = getSyncState();
		syncState.delay = poller.delay;

		int tryCount = 10;
		bool syncSuccess = false;
		while(tryCount-- && !syncSuccess)
		{
			Endpoint ep = g_NetCore->getPeers()[0]->remote_ep;
			g_NetCore->send(ep, (char*)&syncState, sizeof(syncState));
			log << " > CRC=" << syncState.cdvdCRC << ", ";
			log.write(syncState.biosVersion, sizeof(syncState.biosVersion));
			log << endl;
			EmulatorSyncState remoteSyncState;
			int size = g_NetCore->recv(ep, (char*)&remoteSyncState, sizeof(remoteSyncState), 300);
			int needed = sizeof(remoteSyncState);
			log << "received " << size << "/" << needed << endl;
			if( size == needed )
			{
				log << " < CRC=" << remoteSyncState.cdvdCRC << ", ";
				log.write(remoteSyncState.biosVersion, sizeof(remoteSyncState.biosVersion));
				log << endl;
				std::stringstream ss(string(), ios_base::in + ios_base::out);
				ss << "Errors during netplay init: ";
				if(remoteSyncState.delay > syncState.delay)
				{
					syncState.delay = remoteSyncState.delay;
					poller.delay = syncState.delay;
				}
				poller.info = wxString::Format(wxT("Netplay input delay = %d"), poller.delay);

				if(remoteSyncState.mcd1CRC!= syncState.mcd1CRC || 
					remoteSyncState.mcd2CRC!= syncState.mcd2CRC)
				{
					ss << "Memory card CRC mismatch. Memory cards will be disabled. ";
					GetCorePlugins().Close(PluginId_Mcd);
					poller.error = true;
				}
				if(remoteSyncState.cdvdCRC!= syncState.cdvdCRC)
				{
					ss << "You're trying to boot different games, CRC " << syncState.cdvdCRC << "!=" << remoteSyncState.cdvdCRC << ". ";
					poller.error = true;
					poller.active = false;
				}
				if(memcmp(remoteSyncState.biosVersion, syncState.biosVersion, 
					sizeof(syncState.biosVersion)) != 0)
				{
					ss << "Bios version mismatch. ";
					poller.active = false;
					poller.error = true;
				}
				if(poller.active)
					ss << "Closing.";
				poller.errorMsg = ss.str();
				syncSuccess = true;
				break;
			}
		}
		if(!syncSuccess)
		{
			poller.errorMsg = "No responce from remote endpoint. Closing." ;
			poller.active = false;
		}
	}
	void Init()
	{
		EmuOptionsBackup = g_Conf->EmuOptions;
		Mcd1EnabledBackup = g_Conf->Mcd[0].Enabled;
		Mcd2EnabledBackup = g_Conf->Mcd[1].Enabled;
		ProgLogBoxBackup = g_Conf->ProgLogBox;
		EnableGameFixesBackup = g_Conf->EnableGameFixes;

		g_Conf->Mcd[0].Enabled = true;
		g_Conf->Mcd[1].Enabled = true;

		g_Conf->ProgLogBox.Visible = true;

		g_Conf->EmuOptions.Speedhacks.bitset = 0;
		g_Conf->EmuOptions.Speedhacks.DisableAll();

		g_Conf->EmuOptions.Cpu = Pcsx2Config::CpuOptions();
		g_Conf->EmuOptions.GS = Pcsx2Config::GSOptions();

		g_Conf->EnableGameFixes = true;
		g_Conf->EmuOptions.Gamefixes.bitset = 0;
		g_Conf->EmuOptions.Gamefixes.SkipMPEGHack = 1;

		g_Conf->EmuOptions.EnableCheats = false;

		g_Conf->EmuOptions.CdvdDumpBlocks = false;
		g_Conf->EmuOptions.CdvdVerboseReads = false;
		g_Conf->EmuOptions.Profiler.bitset = 0;
		g_Conf->EmuOptions.Trace.Enabled = false;

		g_Conf->Mcd[0].Enabled = true;
		g_Conf->Mcd[1].Enabled = true;

		g_Conf->ProgLogBox.Visible = true;
	}
	void Close()
	{
		g_NetEnabled = false;
		g_NetCore->endSession();
		g_NetCore.Delete();
		poller.active = false;

		g_Conf->EmuOptions = EmuOptionsBackup;
		g_Conf->Mcd[0].Enabled = Mcd1EnabledBackup;
		g_Conf->Mcd[1].Enabled = Mcd2EnabledBackup;
		g_Conf->ProgLogBox = ProgLogBoxBackup;
		g_Conf->EnableGameFixes = EnableGameFixesBackup;

		Net::RestoreHandlers();
		log.close();
	}
	void ReplaceHandlers()
	{
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

		replaced = true;
	}

	void RestoreHandlers()
	{
		if(replaced)
		{
			replaced = false;
			//PADinit = PADinitBackup;
			PADopen = PADopenBackup;
			PADstartPoll = PADstartPollBackup;
			PADpoll = PADpollBackup;
			PADquery = PADqueryBackup;
			PADkeyEvent = PADkeyEventBackup;
			PADsetSlot = PADsetSlotBackup;
			PADqueryMtap = PADqueryMtapBackup;
			PADupdate = PADupdateBackup;
		}
	}
}
