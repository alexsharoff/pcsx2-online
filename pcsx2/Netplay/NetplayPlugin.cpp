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

#include "zlib\zlib.h"

/*
 * TODO:
 * hook IO, but do not init netplay
 * log IO
 * replays
 * no-ip connection system
 * improve memory card handling
 * matchmaking
*/

//#define CONNECTION_TEST

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
	typedef shoryu::session<Message, EmulatorSyncState> session_type;
	boost::shared_ptr<session_type> _session;
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
		shoryu::prepare_io_service();
		_session.reset(new session_type());
#ifdef CONNECTION_TEST
		_session->send_delay_min(40);
		_session->send_delay_max(80);
		_session->packet_loss(25);
#endif
		if(BindPort(g_Conf->Net.MyPort))
		{
			EmuOptionsBackup = g_Conf->EmuOptions;
			Mcd1EnabledBackup = g_Conf->Mcd[0].Enabled;
			Mcd2EnabledBackup = g_Conf->Mcd[1].Enabled;
			ProgLogBoxBackup = g_Conf->ProgLogBox;
			EnableGameFixesBackup = g_Conf->EnableGameFixes;

			g_Conf->Mcd[0].Enabled = true;
			g_Conf->Mcd[1].Enabled = false;

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

			_sessionEnded = false;
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
		_session.reset();
		_isEnabled = false;
		g_Conf->EmuOptions = EmuOptionsBackup;
		g_Conf->Mcd[0].Enabled = Mcd1EnabledBackup;
		g_Conf->Mcd[1].Enabled = Mcd2EnabledBackup;
		g_Conf->ProgLogBox = ProgLogBoxBackup;
		g_Conf->EnableGameFixes = EnableGameFixesBackup;
		if(mcd_backup.get())
		{
			PS2E_McdSizeInfo info;
			SysPlugins.McdGetSizeInfo(0,0,info);
			size_t size = info.McdSizeInSectors*(info.SectorSize + info.EraseBlockSizeInSectors);
			SysPlugins.McdSave(0,0,mcd_backup.get(), 0, size);
			mcd_backup.reset();
		}
		RestoreHandlers();
	}
	virtual bool IsEnabled()
	{
		return _isEnabled;
	}
	virtual bool BindPort(unsigned short port)
	{
		return _session->bind(port);
	}
	virtual bool Connect(const wxString& ip, unsigned short port, int timeout)
	{
		Console.Warning("NETPLAY: connecting to %s:d using local port %d.",
			g_Conf->Net.RemoteIp, g_Conf->Net.RemotePort, g_Conf->Net.MyPort);
		shoryu::endpoint ep = shoryu::resolve_hostname(std::string(ip.mb_str()));
		ep.port(port);
		auto state = GetSyncState();
		if(state)
		{
			if(!_session->join(ep, *state,
				boost::bind(&NetplayPlugin::CheckSyncStates, this, _1, _2), timeout))
				return false;
			PS2E_McdSizeInfo info;
			SysPlugins.McdGetSizeInfo(0,0,info);
			size_t size = info.McdSizeInSectors*(info.SectorSize + info.EraseBlockSizeInSectors);
			Console.WriteLn("CMD_SIZE=%d",size);
			mcd_backup.reset(new u8[size]);
			SysPlugins.McdRead(0,0, (u8*) mcd_backup.get(), 0, size);
			shoryu::msec timeout_timestamp = shoryu::time_ms() + 30000;
			while(true)
			{
				std::auto_ptr<u8> compressed_data(new u8[size]);
				uLongf pos = 0;

				_session->send();
				shoryu::message_data data;
				if(!_session->get_data(1-_session->side(), data, 50))
				{
					if(timeout_timestamp < shoryu::time_ms())
					{
						Console.Error("NETPLAY: Timeout while synchonizing memory cards.",size);
						EndSession();
						return false;
					}
				}
				else
				{
					if(data.data_length != 9 || memcmp(data.p.get(), "BLOCK_END", 9) != 0)
					{
						Console.WriteLn("Receiving block of size %d",data.data_length);
						std::copy(data.p.get(), data.p.get() + data.data_length, compressed_data.get()+pos);
						pos += data.data_length;
					}
					else
					{
						std::auto_ptr<u8> uncompressed_data(new u8[size]);
						uLongf size_test = size;
						int r = uncompress(uncompressed_data.get(), &size_test, compressed_data.get(), pos);
						if(r != Z_OK)
						{
							Console.Error("NETPLAY: Unable to decompress MCD buffer. Error: %d.", r);
							EndSession();
							return false;
						}
						if(size_test != size)
						{
							Console.Error("NETPLAY: Invalid MCD received from host.");
							EndSession();
							return false;
						}
						SysPlugins.McdSave(0,0,uncompressed_data.get(), 0, size);
						return true;
					}
				}
			}
		}
		return false;
	}
	virtual bool Host(int timeout)
	{
		Console.Warning("NETPLAY: hosting using local port %d.", g_Conf->Net.MyPort);
		auto state = GetSyncState();
		if(state)
		{
			if(!_session->create(2, *state,
				boost::bind(&NetplayPlugin::CheckSyncStates, this, _1, _2), timeout))
				return false;
			PS2E_McdSizeInfo info;
			SysPlugins.McdGetSizeInfo(0,0,info);
			uLongf size = info.McdSizeInSectors*(info.SectorSize + info.EraseBlockSizeInSectors);
			std::auto_ptr<Bytef> buffer(new Bytef[size]);
			Console.WriteLn("CMD_SIZE=%d",size);
			{
				std::auto_ptr<Bytef> mcd(new Bytef[size]);
				SysPlugins.McdRead(0,0, (u8*) mcd.get(), 0, size);
				uLongf size_tmp = size;
				int r = compress2(buffer.get(), &size, mcd.get(), size_tmp, Z_BEST_COMPRESSION);
				if(r != Z_OK)
				{
					Console.Error("NETPLAY: Unable to compress MCD buffer. Error: %d.", r);
					EndSession();
					return false;
				}
			}
			Console.WriteLn("COMPRESSED_SIZE=%d",size);
			size_t blockSize = 128;
			for(size_t i = 0; i < size; i+=blockSize)
			{
				shoryu::message_data data;
				data.data_length = size - i;
				if(data.data_length > blockSize)
					data.data_length = blockSize;
				data.p.reset(new char[data.data_length]);
				std::copy(buffer.get()+i, buffer.get()+i+data.data_length, data.p.get());
				Console.WriteLn("Pushing block of size %d",data.data_length);
				_session->queue_data(data);
			}
			shoryu::message_data data;
			data.data_length = 9;
			data.p.reset(new char[data.data_length]);
			char* blockEndMsg = "BLOCK_END";
			std::copy(blockEndMsg, blockEndMsg + data.data_length, data.p.get());
			Console.WriteLn("Pushing block of size %d",data.data_length);
			_session->queue_data(data);

			shoryu::msec timeout_timestamp = shoryu::time_ms() + 30000;
			while(int n = _session->send())
			{
				Console.WriteLn("Sending %d packets", n);
				if(timeout_timestamp < shoryu::time_ms())
				{
					Console.Error("NETPLAY: Timeout while synchonizing memory cards.");
					EndSession();
					return false;
				}
				shoryu::sleep(50);
			}
			return true;
		}
		return false;
	}
	virtual void EndSession()
	{
		if(_session->state() == shoryu::Ready)
		{
			_session->next_frame();
			Message f;
			f.end_session = true;
			_session->set(f);
			int try_count = _session->delay() * 4;
			while(_session->send())
			{
				shoryu::sleep(17);
				if(try_count-- == 0)
					break;
			}
		}
		_session->shutdown();
		_session->unbind();
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
							synchronized = _session->first_received_frame() > 0
								&& _session->frame() > _session->first_received_frame()
								&& _session->frame() <= _session->last_received_frame();

							if(_session->first_received_frame() >= 0)
							{
								_session->get(1-_session->side(), f, _session->first_received_frame()+_session->delay());
								if(f.end_session)
								{
									CoreThread.Reset();
									Console.Warning("NETPLAY: Session ended.");
									UI_EnableEverything();
									_sessionEnded = true;
								}
							}
						}
						if(poller.pollCounter == 7)
						{
							try
							{
								_session->set(myFrame);
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
						if(_session->frame() > _session->last_received_frame())
								shoryu::sleep(17);
						r = f.input[poller.pollCounter-2];
					}
				}
				try
				{
					if(synchronized)
					{
						while(!_session->get(poller.side, f, _session->delay()*17))
						{
							_session->send();
#ifdef CONNECTION_TEST
							shoryu::sleep(3000);
#endif
						}
					}
					else
					{
						_session->send();
					}
					if(f.end_session && !_sessionEnded)
					{
						CoreThread.Reset();
						Console.Warning("NETPLAY: Session ended.");
						UI_EnableEverything();
						_sessionEnded = true;
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
				if(_thread->timed_join(boost::posix_time::milliseconds(1000)))
				{
					if(_session->state() == shoryu::Ready)
					{
						std::string ip = _session->endpoints()[0].address().to_string();
						int port = _session->endpoints()[0].port();
						int delay = _session->delay();
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
					_session->next_frame();
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
		/*if(s1.mcd1CRC != s2.mcd1CRC)
		{
			Console.Warning("NETPLAY: Memory card 1 CRC mismatch. Memory cards will be disabled.",
				s1.mcd1CRC, s2.mcd1CRC);
			CoreThread.CloseMcdPlugin();
		}
		else
			if(s1.mcd2CRC != s2.mcd2CRC)
			{
				Console.Warning("NETPLAY: Memory card 2 CRC mismatch. Memory cards will be disabled.",
					s1.mcd2CRC, s2.mcd2CRC);
				CoreThread.CloseMcdPlugin();
			}*/
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
	bool _sessionEnded;

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
	std::auto_ptr<u8> mcd_backup;



	boost::shared_ptr<EmulatorSyncState> GetSyncState()
	{
		boost::shared_ptr<EmulatorSyncState> syncState(new EmulatorSyncState());

		cdvdReloadElfInfo();
		if(DiscSerial.Len() == 0)
		{
			CoreThread.Reset();
			Console.Error("NETPLAY: Unable to obtain disc serial.");
			UI_EnableEverything();
			syncState.reset();
			return syncState;
		}

		memset(syncState->discSerial, 0, sizeof(syncState->discSerial));
		memcpy(syncState->discSerial, DiscSerial.ToAscii().data(), 
			DiscSerial.length() > sizeof(syncState->discSerial) ? 
			sizeof(syncState->discSerial) : DiscSerial.length());

		wxString biosDesc;
		if(!IsBIOS(g_Conf->EmuOptions.BiosFilename.GetFullPath(), biosDesc))
		{
			CoreThread.Reset();
			Console.Error("NETPLAY: Unable to check BIOS.");
			UI_EnableEverything();
			syncState.reset();
			return syncState;
		}

		memset(syncState->biosVersion, 0, sizeof(syncState->biosVersion));
		memcpy(syncState->biosVersion, biosDesc.ToAscii().data(), 
			biosDesc.length() > sizeof(syncState->biosVersion) ? 
			sizeof(syncState->biosVersion) : biosDesc.length());
		try
		{
			syncState->mcd1CRC = SysPlugins.McdGetCRC( 0, 0 );
			syncState->mcd2CRC = SysPlugins.McdGetCRC( 1, 0 );
		}
		catch(...)
		{
			CoreThread.Reset();
			Console.Error("NETPLAY: Unable to read memory cards.");
			UI_EnableEverything();
			syncState.reset();
		}
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
