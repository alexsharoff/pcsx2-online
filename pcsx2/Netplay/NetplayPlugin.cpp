#include "PrecompiledHeader.h"

#include "AppConfig.h"
#include <iostream>
#include <fstream>
#include "ps2/BiosTools.h"
#include "Elfheader.h"
#include "CDVD/CDVD.h"
#include "Netplay/NetplayPlugin.h"
#include "Netplay/INetplayDialog.h"

#include "shoryu/session.h"
#include "Message.h"

#include "NetplaySettings.h"

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

class NetplayPlugin : public INetplayPlugin
{
	typedef shoryu::session<Message, EmulatorSyncState> session_type;
	boost::shared_ptr<session_type> _session;
	boost::shared_ptr<boost::thread> _thread;
public:
	NetplayPlugin() : _isEnabled(false){}
	virtual void Open()
	{
		_error_string = "";
		_info_string = "";
		NetplaySettings& settings = g_Conf->Net;
		if( settings.LocalPort <= 0 || settings.LocalPort > 65535 )
		{
			CoreThread.Reset();
			Console.Error("NETPLAY: Invalid local port: %u.", settings.LocalPort);
			UI_EnableEverything();
			return;
		}
		if( settings.Mode == ConnectMode && (settings.HostPort <= 0 || settings.HostPort > 65535) )
		{
			CoreThread.Reset();
			Console.Error("NETPLAY: Invalid remove port: %u.", settings.HostPort);
			UI_EnableEverything();
			return;
		}
		if( settings.Mode == ConnectMode && settings.HostAddress.Len() == 0 )
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
		if(BindPort(settings.LocalPort))
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
			_connectionEstablished = false;
			_session->username(std::string((const char*)settings.Username.mb_str(wxConvUTF8)));
			if(settings.Mode == ConnectMode)
			{
				Console.Warning("NETPLAY: Connecting to %s:%u using local port %u.",
					settings.HostAddress.ToAscii().data(), settings.HostPort, settings.LocalPort);
				_thread.reset(new boost::thread(boost::bind(&NetplayPlugin::Connect,
					this, settings.HostAddress, settings.HostPort, 0)));
			}
			else
			{
				Console.Warning("NETPLAY: Hosting on local port %u.", settings.LocalPort);
				_thread.reset(new boost::thread(boost::bind(&NetplayPlugin::Host, this, 0)));
			}
			ReplaceHandlers();
		}
		else
		{
			CoreThread.Reset();
			Console.Error("NETPLAY: Unable to bind port %u.", settings.LocalPort);
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
			ResetMemoryCard(0,0,mcd_backup.get(), size);
			mcd_backup.reset();
		}
		RestoreHandlers();
	}
	virtual void Enable(bool enabled)
	{
		_isEnabled = enabled;
	}
	virtual bool IsEnabled()
	{
		return _isEnabled;
	}
	virtual bool BindPort(unsigned short port)
	{
		return _session->bind(port);
	}
	void ResetMemoryCard(int port, int slot, const u8* data, size_t size)
	{
		for(size_t p = 0; p < size; p+= 528*16)
			SysPlugins.McdEraseBlock(0,0,p);

		SysPlugins.McdSave(port,slot, data, 0, size);
	}

	virtual bool Connect(const wxString& ip, unsigned short port, int timeout)
	{
		shoryu::endpoint ep = shoryu::resolve_hostname(std::string(ip.ToAscii().data()));
		ep.port(port);
		auto state = GetSyncState();
		if(state)
		{
			if(!_session->join(ep, *state,
				boost::bind(&NetplayPlugin::CheckSyncStates, this, _1, _2), timeout))
				return false;
			INetplayDialog* dialog = INetplayDialog::GetInstance();
			ExecuteOnMainThread([&]() {
				dialog->OnConnectionEstablished(_session->delay());
			});
			std::string player_name;
			{
				boost::unique_lock<boost::mutex> lock(_string_mutex);
				{
					auto ep = _session->endpoints()[0];
					player_name = _session->username(ep);
					if(!player_name.length())
					{
						std::stringstream ss(std::ios_base::in + std::ios_base::out);
						ss << ep.address().to_string() << ":" << ep.port();
						player_name = ss.str();
					}
				}
				{
					std::stringstream ss(std::ios_base::in + std::ios_base::out);
					ss << "NETPLAY: Connected to " << player_name << ". Starting memory card synchronization.";
					_info_string = ss.str();
				}
			}
			ExecuteOnMainThread([&]() {
				dialog->SetStatus(wxT("Connected to ") + 
					wxString(player_name.c_str(), wxConvUTF8));
			});

			int delay = dialog->WaitForConfirmation();
			if(delay <= 0)
				return false;

			ExecuteOnMainThread([&]() {
				dialog->SetStatus(wxT("Memory card synchronization..."));
			});

			PS2E_McdSizeInfo info;
			SysPlugins.McdGetSizeInfo(0,0,info);
			size_t size = info.McdSizeInSectors*(info.SectorSize + info.EraseBlockSizeInSectors);
			//Console.WriteLn("MCD_SIZE=%d",size);
			mcd_backup.reset(new u8[size]);
			SysPlugins.McdRead(0,0, (u8*) mcd_backup.get(), 0, size);
			shoryu::msec timeout_timestamp = shoryu::time_ms() + 30000;
			boost::shared_array<u8> compressed_data(new u8[size]);
			uLongf pos = 0;

			while(true)
			{
				if(_session->end_session_request())
					return false;
				if(!_isEnabled)
					return false;
				shoryu::message_data data;
				if(!_session->get_data(1-_session->side(), data, 50))
				{
					if(timeout_timestamp < shoryu::time_ms())
					{
						boost::unique_lock<boost::mutex> lock(_string_mutex);
						_error_string = "NETPLAY: Timeout while synchonizing memory cards.";
						EndSession();
						return false;
					}
				}
				else
				{
					if(data.data_length != 9 || memcmp(data.p.get(), "BLOCK_END", 9) != 0)
					{
						//Console.WriteLn("Receiving block of size %d",data.data_length);
						std::copy(data.p.get(), data.p.get() + data.data_length, compressed_data.get()+pos);
						pos += data.data_length;
					}
					else
					{
						boost::shared_array<u8> uncompressed_data(new u8[size]);
						uLongf size_test = size;
						/*Console.WriteLn("COMPRESSED_SIZE=%d",pos);
						{
							std::fstream fs("compressed.z", std::ios_base::out + std::ios_base::binary);
							fs.write((char*)compressed_data.get(), pos);
						}*/
						int r = uncompress(uncompressed_data.get(), &size_test, compressed_data.get(), pos);
						if(r != Z_OK)
						{
							boost::unique_lock<boost::mutex> lock(_string_mutex);
							std::stringstream ss(std::ios_base::in + std::ios_base::out);
							ss << "NETPLAY: Unable to decompress MCD buffer. Error: " << r;
							_error_string = ss.str();
							EndSession();
							return false;
						}
						if(size_test != size)
						{
							boost::unique_lock<boost::mutex> lock(_string_mutex);
							_error_string = "NETPLAY: Invalid MCD received from host.";
							EndSession();
							return false;
						}
						ResetMemoryCard(0,0,uncompressed_data.get(), size);
						break;
					}
				}
				_session->send();
				if(delay != _session->delay())
				{
					ExecuteOnMainThread([&]() {
						if(dialog->IsShown())
							dialog->SetInputDelay(_session->delay());
					});
				}
			}
			timeout_timestamp = shoryu::time_ms() + 3000;
			while(true)
			{
				if(_session->end_session_request())
					return false;
				_session->send();
				if(timeout_timestamp < shoryu::time_ms())
					break;
				shoryu::sleep(50);
			}
			_connectionEstablished = true;
			return true;
		}
		return false;
	}
	virtual bool Host(int timeout)
	{
		auto state = GetSyncState();
		if(state)
		{
			if(!_session->create(2, *state,
				boost::bind(&NetplayPlugin::CheckSyncStates, this, _1, _2), timeout))
				return false;
			INetplayDialog* dialog = INetplayDialog::GetInstance();
			ExecuteOnMainThread([&]() {
				dialog->OnConnectionEstablished(_session->delay());
			});
			std::string player_name;
			{
				boost::unique_lock<boost::mutex> lock(_string_mutex);
				{
					auto ep = _session->endpoints()[0];
					player_name = _session->username(ep);
					if(!player_name.length())
					{
						std::stringstream ss(std::ios_base::in + std::ios_base::out);
						ss << ep.address().to_string() << ":" << ep.port();
						player_name = ss.str();
					}
				}
				{
					std::stringstream ss(std::ios_base::in + std::ios_base::out);
					ss << "NETPLAY: Connection from " << player_name << ". Starting memory card synchronization.";
					_info_string = ss.str();
				}
			}
			ExecuteOnMainThread([&]() {
				dialog->SetStatus(wxT("Connection from ") + 
					wxString(player_name.c_str(), wxConvUTF8));
			});
			int delay = dialog->WaitForConfirmation();
			if(delay <= 0)
				return false;
			if(delay != _session->delay())
			{
				_session->delay(delay);
				_session->reannounce_delay();
			}
			ExecuteOnMainThread([&]() {
				dialog->SetStatus(wxT("Memory card synchronization..."));
			});
			PS2E_McdSizeInfo info;
			SysPlugins.McdGetSizeInfo(0,0,info);
			uLongf size = info.McdSizeInSectors*(info.SectorSize + info.EraseBlockSizeInSectors);
			boost::shared_array<Bytef> buffer(new Bytef[size]);
			//Console.WriteLn("MCD_SIZE=%d",size);
			{
				boost::shared_array<Bytef> mcd(new Bytef[size]);
				SysPlugins.McdRead(0,0, (u8*) mcd.get(), 0, size);

				if(g_Conf->Net.ReadonlyMemcard)
					mcd_backup = mcd;

				uLongf size_tmp = size;
				int r = compress2(buffer.get(), &size, mcd.get(), size_tmp, Z_BEST_COMPRESSION);
				if(r != Z_OK)
				{
					boost::unique_lock<boost::mutex> lock(_string_mutex);
					std::stringstream ss(std::ios_base::in + std::ios_base::out);
					ss << "NETPLAY: Unable to compress MCD buffer. Error: " << r;
					_error_string = ss.str();
					EndSession();
					return false;
				}
			}
			/*Console.WriteLn("COMPRESSED_SIZE=%d",size);
			{
				std::fstream fs("compressed.z", std::ios_base::out + std::ios_base::binary);
				fs.write((char*)buffer.get(), size);
				uLongf size_tmp = 9000000;
				Bytef* d = new Bytef[size_tmp];
				int r = uncompress(d, &size_tmp, buffer.get(), size);
				if(r != Z_OK)
				{
					Console.Error("NETPLAY: Unable to decompress MCD buffer. Error: %d.", r);
				}
				else
				{
					Console.WriteLn("UNCOMPRESSED_SIZE=%d",size_tmp);
				}
				delete[] d;
			}*/
			size_t blockSize = 128;
			for(size_t i = 0; i < size; i+=blockSize)
			{
				shoryu::message_data data;
				data.data_length = size - i;
				if(data.data_length > blockSize)
					data.data_length = blockSize;
				data.p.reset(new char[data.data_length]);
				std::copy(buffer.get()+i, buffer.get()+i+data.data_length, data.p.get());
				//Console.WriteLn("Pushing block of size %d",data.data_length);
				_session->queue_data(data);
			}
			shoryu::message_data data;
			data.data_length = 9;
			data.p.reset(new char[data.data_length]);
			char* blockEndMsg = "BLOCK_END";
			std::copy(blockEndMsg, blockEndMsg + data.data_length, data.p.get());
			//Console.WriteLn("Pushing block of size %d",data.data_length);
			_session->queue_data(data);

			shoryu::msec timeout_timestamp = shoryu::time_ms() + 30000;
			while(int n = _session->send_sync())
			{
				if(_session->end_session_request())
					return false;
				if(_session->first_received_frame() != -1)
				{
					_session->clear_queue();
					break;
				}
				if(!_isEnabled)
					return false;
				//Console.WriteLn("Sending %d packets", n);
				if(timeout_timestamp < shoryu::time_ms())
				{
					boost::unique_lock<boost::mutex> lock(_string_mutex);
					_error_string = "NETPLAY: Timeout while synchonizing memory cards.";
					EndSession();
					return false;
				}
			}
			_connectionEstablished = true;
			return true;
		}
		return false;
	}
	void EndSession()
	{
		ExecuteOnMainThread([&]() {
			INetplayDialog* dialog = INetplayDialog::GetInstance();
			if(dialog->IsShown())
				dialog->Close();
		});
		if(_session)
		{
			if(_session->state() == shoryu::Ready)
			{
				_session->send_end_session_request();
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
	}


	virtual u8 CALLBACK NETPADpoll(u8 value)
	{
		if(poller.pollCounter == 0 && value == 0x42 && poller.side == 0)
			poller.cmd42Counter++;

		u8 r = PADpollBackup(value);

		if(_session && _session->end_session_request() && !_sessionEnded)
		{
			ExecuteOnMainThread([&]() {
				INetplayDialog* dialog = INetplayDialog::GetInstance();
				if(dialog->IsShown())
					dialog->Close();
			});
			if(_thread)
				_thread->join();
			CoreThread.Reset();
			Console.Warning("NETPLAY: Session ended.");
			UI_EnableEverything();
			_sessionEnded = true;
		}
		if(_sessionEnded)
			return r;
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
						auto timeout = shoryu::time_ms() + 10000;
						while(!_session->get(poller.side, f, _session->delay()*17))
						{
							_session->send();
							if(_session->end_session_request())
								break;
							if(timeout <= shoryu::time_ms())
							{
								CoreThread.Reset();
								Console.Warning("NETPLAY: Timeout.");
								ExecuteOnMainThread([&]() {
									INetplayDialog* dialog = INetplayDialog::GetInstance();
									if(dialog->IsShown())
										dialog->Close();
								});
								UI_EnableEverything();
								_sessionEnded = true;
								break;
							}
#ifdef CONNECTION_TEST
							shoryu::sleep(3000);
#endif
						}
					}
					else
					{
						_session->send();
					}
				}
				catch(std::exception& e)
				{
					EndSession();
					CoreThread.Reset();
					Console.Error("NETPLAY: %s", e.what());
					UI_EnableEverything();
				}
				r = f.input[poller.pollCounter-2];
			}
			else
				r = 0xff;
		}
		poller.pollCounter++;
		return r;
	}
	
	virtual s32 CALLBACK NETPADsetSlot(u8 port, u8 slot)
	{
		{
			boost::unique_lock<boost::mutex> lock(_string_mutex);
			if(_info_string.length())
			{
				Console.WriteLn(Color_StrongGreen, _info_string.c_str());
				_info_string = "";
			}
			if(_error_string.length())
			{
				Console.Error(_error_string.c_str());
				_error_string = "";
			}
			
		}
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
				if(_thread->timed_join(boost::posix_time::milliseconds(3000)))
				{
					if(_session->state() != shoryu::Ready)
					{
						CoreThread.Reset();
						Console.Error("NETPLAY: Unable to establish connection.");
						UI_EnableEverything();
						return PADsetSlotBackup(port, slot);
					}
					if(_connectionEstablished)
					{
						ExecuteOnMainThread([&]() {
							INetplayDialog* dialog = INetplayDialog::GetInstance();
							if(dialog->IsShown())
								dialog->Close();
						});
						Console.WriteLn(Color_StrongGreen, "NETPLAY: Delay %d. Starting netplay.", _session->delay());
						ready = true;
					}
					else
					{
						CoreThread.Reset();
						Console.Error("NETPLAY: Interrupted.");
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
			if(_session->last_error().length())
			{
				// 'The operation completed successfully' is a really strange error.
				// Research it!
				// Console.Error("NETPLAY: %s.", _session->last_error().c_str());
				// _session->last_error("");
			}
		}
		return PADsetSlotBackup(port, slot);
	}

protected:
	static std::function<void()> _dispatch_event;
	static void DispatchEvent()
	{
		if(_dispatch_event)
			_dispatch_event();
	}
	void ExecuteOnMainThread(const std::function<void()>& evt)
	{
		if(!_dispatch_event)
			_dispatch_event = evt;
		if (!wxGetApp().Rpc_TryInvoke( DispatchEvent ))
			ExecuteOnMainThread(evt);
		if(_dispatch_event)
			_dispatch_event = std::function<void()>();
	}

	bool CheckSyncStates(const EmulatorSyncState& s1, const EmulatorSyncState& s2)
	{
		if(memcmp(s1.biosVersion, s2.biosVersion, sizeof(s1.biosVersion)))
		{
			boost::unique_lock<boost::mutex> lock(_string_mutex);
			_error_string = "NETPLAY: Bios version mismatch.";
			return false;
		}
		if(memcmp(s1.discSerial, s2.discSerial, sizeof(s1.discSerial)))
		{
			boost::unique_lock<boost::mutex> lock(_string_mutex);
			std::stringstream ss(std::ios_base::in + std::ios_base::out);
			ss << "NETPLAY: You are trying to boot different games ("<<s1.discSerial<<" != "<<s2.discSerial<<").";
			_error_string = ss.str();
			return false;
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
	bool _sessionEnded;
	bool _connectionEstablished;

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
	boost::shared_array<u8> mcd_backup;
	std::string _error_string;
	std::string _info_string;
	boost::mutex _string_mutex;

	boost::shared_ptr<EmulatorSyncState> GetSyncState()
	{
		boost::shared_ptr<EmulatorSyncState> syncState(new EmulatorSyncState());

		cdvdReloadElfInfo();
		if(DiscSerial.Len() == 0)
		{
			CoreThread.Reset();
			boost::unique_lock<boost::mutex> lock(_string_mutex);
			_error_string = "NETPLAY: Unable to obtain disc serial.";
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
			boost::unique_lock<boost::mutex> lock(_string_mutex);
			_error_string = "NETPLAY: Unable to check BIOS.";
			UI_EnableEverything();
			syncState.reset();
			return syncState;
		}
		memset(syncState->biosVersion, 0, sizeof(syncState->biosVersion));
		memcpy(syncState->biosVersion, biosDesc.ToAscii().data(), 
			biosDesc.length() > sizeof(syncState->biosVersion) ? 
			sizeof(syncState->biosVersion) : biosDesc.length());
		return syncState;
	}
};

std::function<void()> NetplayPlugin::_dispatch_event = std::function<void()>();

INetplayPlugin* INetplayPlugin::instance = 0;

INetplayPlugin& INetplayPlugin::GetInstance()
{
	if(!instance)
		instance = new NetplayPlugin();
	return *instance;
}
