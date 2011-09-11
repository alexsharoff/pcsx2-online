#include "PrecompiledHeader.h"

#include "AppConfig.h"
#include <iostream>
#include <fstream>
#include "ps2/BiosTools.h"
#include "Elfheader.h"
#include "CDVD/CDVD.h"
#include "Netplay/NetplayPlugin.h"
#include "Netplay/gui/NetplayDialog.h"
#include "Netplay/gui/ConnectionStatusDialog.h"

#include "shoryu/session.h"
#include "Message.h"

using namespace std;
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
	
	s32 CALLBACK NETPADopen(void *pDsp)
	{
		return INetplayPlugin::GetInstance().NETPADopen(pDsp);
	}
	u8 CALLBACK NETPADstartPoll(int pad)
	{
		return INetplayPlugin::GetInstance().NETPADstartPoll(pad);
	}
	u8 CALLBACK NETPADpoll(u8 value)
	{
		return INetplayPlugin::GetInstance().NETPADpoll(value);
	}
	u32 CALLBACK NETPADquery(int pad)
	{
		return INetplayPlugin::GetInstance().NETPADquery(pad);
	}
	keyEvent* CALLBACK NETPADkeyEvent()
	{
		return INetplayPlugin::GetInstance().NETPADkeyEvent();
	}
	s32 CALLBACK NETPADsetSlot(u8 port, u8 slot)
	{
		return INetplayPlugin::GetInstance().NETPADsetSlot(port, slot);
	}
	s32 CALLBACK NETPADqueryMtap(u8 port)
	{
		return INetplayPlugin::GetInstance().NETPADqueryMtap(port);
	}
	void CALLBACK NETPADupdate(int pad)
	{
		return INetplayPlugin::GetInstance().NETPADupdate(pad);
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
	}

	void RestoreHandlers()
	{
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

class NetplayPlugin : public INetplayPlugin
{
	shoryu::session<Message, EmulatorSyncState> _session;
	boost::shared_ptr<boost::thread> _thread;
public:
	NetplayPlugin() : _isEnabled(false){}
	virtual void Open()
	{
		if( g_Conf->Net.MyPort <= 0 || g_Conf->Net.MyPort > 65535 )
		{
			CoreThread.Reset();
			Console.Error("NETPLAY: Invalid local port: %d.", g_Conf->Net.MyPort);
			UI_EnableEverything();
			return;
		}
		if( g_Conf->Net.Connect && (g_Conf->Net.RemotePort <= 0 || g_Conf->Net.RemotePort > 65535) )
		{
			CoreThread.Reset();
			Console.Error("NETPLAY: Invalid remove port: %d.", g_Conf->Net.RemotePort);
			UI_EnableEverything();
			return;
		}
		if( g_Conf->Net.Connect && g_Conf->Net.RemoteIp.Len() == 0 )
		{
			CoreThread.Reset();
			Console.Error("NETPLAY: Invalid hostname.");
			UI_EnableEverything();
			return;
		}
		if(BindPort(g_Conf->Net.MyPort))
		{
			EmuOptionsBackup = g_Conf->EmuOptions;
			Mcd1EnabledBackup = g_Conf->Mcd[0].Enabled;
			Mcd2EnabledBackup = g_Conf->Mcd[1].Enabled;
			ProgLogBoxBackup = g_Conf->ProgLogBox;
			EnableGameFixesBackup = g_Conf->EnableGameFixes;

			g_Conf->Mcd[0].Enabled = true;
			g_Conf->Mcd[1].Enabled = true;

			g_Conf->ProgLogBox.Visible = true;

			g_Conf->EmuOptions.HostFs = false;
			g_Conf->EmuOptions.McdEnableEjection = false;
			g_Conf->EmuOptions.Profiler.bitset = 0;
			g_Conf->EmuOptions.UseBOOT2Injection = true;
			g_Conf->EmuOptions.EnablePatches = false;
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

			first = true;
			synchronized = false;
			ready = false;
			if(g_Conf->Net.Connect)
				_thread.reset(new boost::thread(boost::bind(&NetplayPlugin::Connect,
					this, g_Conf->Net.RemoteIp, g_Conf->Net.RemotePort, 0)));
			else
				_thread.reset(new boost::thread(boost::bind(&NetplayPlugin::Host, this, 0)));
			ReplaceHandlers();
		}
		else
		{
			CoreThread.Reset();
			Console.Error("NETPLAY: unable to bind port %d.", g_Conf->Net.MyPort);
			UI_EnableEverything();
		}
	}
	virtual void Init()
	{
	}
	virtual void Close()
	{
		EndSession();
		_isEnabled = false;
		g_Conf->EmuOptions = EmuOptionsBackup;
		g_Conf->Mcd[0].Enabled = Mcd1EnabledBackup;
		g_Conf->Mcd[1].Enabled = Mcd2EnabledBackup;
		g_Conf->ProgLogBox = ProgLogBoxBackup;
		g_Conf->EnableGameFixes = EnableGameFixesBackup;

		RestoreHandlers();
	}
	virtual bool IsEnabled()
	{
		return _isEnabled;
	}
	virtual bool BindPort(unsigned short port)
	{
		return _session.bind(port);
	}
	virtual bool Connect(const wxString& ip, unsigned short port, int timeout)
	{
		Console.Warning("NETPLAY: connecting to %s:d using local port %d.",
			g_Conf->Net.RemoteIp, g_Conf->Net.RemotePort, g_Conf->Net.MyPort);
		shoryu::endpoint ep = shoryu::resolve_hostname(std::string(ip.mb_str()));
		ep.port(port);
		return _session.join(ep, GetSyncState(),
			boost::bind(&NetplayPlugin::CheckSyncStates, this, _1, _2), timeout);
	}
	virtual bool Host(int timeout)
	{
		Console.Warning("NETPLAY: hosting using local port %d.", g_Conf->Net.MyPort);
		return _session.create(2, GetSyncState(),
			boost::bind(&NetplayPlugin::CheckSyncStates, this, _1, _2), timeout);
	}
	virtual void EndSession()
	{
		_session.shutdown();
		_session.unbind();
	}
	
	virtual void ShowNetplayDialog(AppConfig::NetOptions& options)
	{
		NetplayDialog dialog(options, (wxWindow*)GetMainFramePtr());
		if(dialog.ShowModal() == wxID_OK)
		{
			_isEnabled = true;
			sApp.SysExecute( g_Conf->CdvdSource );
			UI_DisableEverything();
		}
	}
	virtual s32 CALLBACK NETPADopen(void *pDsp)
	{
		return PADopenBackup(pDsp);
	}
	virtual u8 CALLBACK NETPADstartPoll(int pad)
	{
		u8 r = PADstartPollBackup(pad);
		return r;
	}
	virtual u8 CALLBACK NETPADpoll(u8 value)
	{
		if(poller.pollCounter == 0 && value == 0x42 && poller.side == 0)
			poller.cmd42Counter++;

		u8 r = PADpollBackup(value);
		Message f;
		if(!IsEnabled())
		{
			RestoreHandlers();
			CoreThread.Reset();
			UI_EnableEverything();
			return r;
		}

		if(poller.cmd42Counter > 2 && poller.pollCounter > 1)
		{
			if(poller.pollCounter <=7)
			{
				if(poller.side == 0)
				{
					myFrame.input[poller.pollCounter-2] = r;
					if(ready)
					{
						if(!synchronized)
						{
							synchronized = _session.first_received_frame() > 0
								&& _session.frame() > _session.first_received_frame()
								&& _session.frame() <= _session.last_received_frame();
						}
						if(poller.pollCounter == 7)
						{
							try
							{
								_session.set(myFrame);
							}
							catch(std::exception& e)
							{
								Console.Error("NETPLAY: %s. Disabling netplay.", e.what());
								this->Close();
								return r;
							}
						}
					}
					if(!synchronized)
					{
						if(_session.frame() > _session.last_received_frame())
								shoryu::sleep(100);
						r = f.input[poller.pollCounter-2];
					}
				}
				try
				{
					if(synchronized)
					{
						if(!_session.get(poller.side, f, _session.delay()*100))
						{
							_session.send();
							using std::ios_base;
							std::stringstream ss(ios_base::in + ios_base::out);
							ss << "NETPLAY: timeout on frame "<<_session.frame()<<
								" (last received frame - "<<_session.last_received_frame()<<").";
							Console.Error(ss.str().c_str() );
						}
					}
					else
					{
						_session.send();
					}
				}
				catch(std::exception& e)
				{
				}
				r = f.input[poller.pollCounter-2];
			}
			else
				r = 0xff;
		}
		poller.pollCounter++;
		return r;
	}
	virtual u32 CALLBACK NETPADquery(int pad)
	{
		return PADqueryBackup(pad);
	}
	virtual keyEvent* CALLBACK NETPADkeyEvent()
	{
		return PADkeyEventBackup();
	}
	virtual s32 CALLBACK NETPADsetSlot(u8 port, u8 slot)
	{
		if(!IsEnabled())
		{
			RestoreHandlers();
			CoreThread.Reset();
			UI_EnableEverything();
		}
		else
		{
			if(!ready && port == 1)
			{
				if(_thread->timed_join(boost::posix_time::milliseconds(100)))
				{
					if(_session.state() != shoryu::None)
					{
						std::string ip = _session.endpoints()[0].address().to_string();
						int port = _session.endpoints()[0].port();
						int delay = _session.delay();
						Console.WriteLn(Color_StrongGreen, "NETPLAY: Connection from %s:%d, delay %d", ip.c_str(), port, delay);
						ready = true;
					}
					else
					{
						CoreThread.Reset();
						Console.Error("NETPLAY: Unable to establish connection.");
						UI_EnableEverything();
						return PADsetSlotBackup(port, slot);
					}
				}
			}
			if(!first && port == 1)
			{
				if(poller.cmd42Counter > 2)
					_session.next_frame();
			}
			poller.pollCounter = 0;
			poller.side = port-1;
			if(poller.cmd42Counter >=2 && poller.side == 0)
				myFrame = Message();
			first = false;
		}
		return PADsetSlotBackup(port, slot);
	}
	virtual s32 CALLBACK NETPADqueryMtap(u8 port)
	{
		return PADqueryMtapBackup(port);
	}
	virtual void CALLBACK NETPADupdate(int pad)
	{
		if(PADupdateBackup)
			PADupdateBackup(pad);
	}

protected:
	bool CheckSyncStates(const EmulatorSyncState& s1, const EmulatorSyncState& s2)
	{
		if(memcmp(s1.biosVersion, s2.biosVersion, sizeof(s1.biosVersion)))
		{
			Console.Error("NETPLAY: Bios version mismatch.");
			return false;
		}
		if(memcmp(s1.discSerial, s2.discSerial, sizeof(s1.discSerial)))
		{
			Console.Error("NETPLAY: You're trying to boot different games (%s != %s).", s1.discSerial, s2.discSerial);
			return false;
		}
		if(s1.mcd1CRC != s2.mcd1CRC)
		{
			Console.Warning("NETPLAY: Memory card 1 CRC mismatch (CRC %d != %d). Memory cards will be disabled.",
				s1.mcd1CRC, s2.mcd1CRC);
			GetCorePlugins().Close(PluginId_Mcd);
		}
		else
			if(s1.mcd2CRC != s2.mcd2CRC)
			{
				Console.Warning("NETPLAY: Memory card 2 CRC mismatch (CRC %d != %d). Memory cards will be disabled.",
					s1.mcd2CRC, s2.mcd2CRC);
				GetCorePlugins().Close(PluginId_Mcd);
			}
		return true;
	}

	//rename to InputStateMachine and remove excess fields
	struct padPoller {
	public:
		padPoller() {
			pollCounter = 0;
			pressureMode = false;
			frame = 0;
			cmd42Counter = 0;
			lastSent = -1;
			side = 0;
		}
		int side;
		int pollCounter;
		bool pressureMode;
		int frame;
		int cmd42Counter;
		bool delayBufferReady;
		int lastSent;
		int mySide;
	} poller;

	bool _isEnabled;

	Message myFrame;
	Pcsx2Config EmuOptionsBackup;
	bool Mcd1EnabledBackup;
	bool Mcd2EnabledBackup;
	bool EnableGameFixesBackup;
	AppConfig::ConsoleLogOptions ProgLogBoxBackup;
	bool replaced;
	bool first;
	bool ready;
	bool synchronized;

	EmulatorSyncState GetSyncState()
	{
		EmulatorSyncState syncState;

		cdvdReloadElfInfo();
		//syncState.cdvdCRC = ElfCRC;

		memset(syncState.discSerial, 0, sizeof(syncState.discSerial));
		memcpy(syncState.discSerial, DiscSerial.ToAscii().data(), 
			DiscSerial.length() > sizeof(syncState.discSerial) ? 
			sizeof(syncState.discSerial) : DiscSerial.length());

		wxString biosDesc;
		IsBIOS(g_Conf->EmuOptions.BiosFilename.GetFullPath(), biosDesc);

		memset(syncState.biosVersion, 0, sizeof(syncState.biosVersion));
		memcpy(syncState.biosVersion, biosDesc.ToAscii().data(), 
			biosDesc.length() > sizeof(syncState.biosVersion) ? 
			sizeof(syncState.biosVersion) : biosDesc.length());
		wxString name;
		GetPS2ElfName(name);
		syncState.mcd1CRC = SysPlugins.McdGetCRC( 0, 0 );
		syncState.mcd2CRC = SysPlugins.McdGetCRC( 1, 0 );
		return syncState;
	}
};

INetplayPlugin* INetplayPlugin::instance = 0;

INetplayPlugin& INetplayPlugin::GetInstance()
{
	if(!instance)
		instance = new NetplayPlugin();
	return *instance;
}