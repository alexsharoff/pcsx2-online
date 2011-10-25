#include "PrecompiledHeader.h"

#include "AppConfig.h"
#include <wx/stdpaths.h>
#include <iostream>
#include <fstream>
#include "ps2/BiosTools.h"
#include "Elfheader.h"
#include "CDVD/CDVD.h"
#include "Netplay/NetplayPlugin.h"
#include "Netplay/INetplayDialog.h"

#include "shoryu/session.h"
#include "Message.h"
#include "Replay.h"

#include "NetplaySettings.h"

#include "Utilities.h"


//#define CONNECTION_TEST

using namespace std;

class NetplayPlugin : public INetplayPlugin
{
	typedef shoryu::session<Message, EmulatorSyncState> session_type;
	boost::shared_ptr<session_type> _session;
	boost::shared_ptr<boost::thread> _thread;
public:
	virtual void Open()
	{
		NetplaySettings& settings = g_Conf->Net;
		if( settings.LocalPort <= 0 || settings.LocalPort > 65535 )
		{
			CoreThread.Reset();
			Console.Error("NETPLAY: Invalid port: %u.", settings.LocalPort);
			UI_EnableEverything();
			return;
		}
		if( settings.Mode == ConnectMode && (settings.HostPort <= 0 || settings.HostPort > 65535) )
		{
			CoreThread.Reset();
			Console.Error("NETPLAY: Invalid port: %u.", settings.HostPort);
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
			synchronized = false;
			_connectionEstablished = false;
			_session->username(std::string((const char*)settings.Username.mb_str(wxConvUTF8)));

			if(g_Conf->Net.SaveReplay)
			{
				_replay.reset(new Replay());
				_replay->Mode(Recording);
			}
			_game_name.clear();
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
		}
		else
		{
			Console.Error("NETPLAY: Unable to bind port %u.", settings.LocalPort);
			Stop();
		}
	}
	virtual void Init()
	{
	}
	virtual void Close()
	{
		EndSession();
		_session.reset();
		g_Conf->EmuOptions = EmuOptionsBackup;
		g_Conf->Mcd[0].Enabled = Mcd1EnabledBackup;
		g_Conf->Mcd[1].Enabled = Mcd2EnabledBackup;
		g_Conf->ProgLogBox = ProgLogBoxBackup;
		g_Conf->EnableGameFixes = EnableGameFixesBackup;
		if(mcd_backup.size())
		{
			Utilities::WriteMCD(0,0,mcd_backup);
			mcd_backup.clear();
		}
		if(_replay)
		{
			try
			{
				wxDirName dir = (wxDirName)wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
				dir = dir.Combine(wxDirName("replays"));
				wxString replayName = _game_name + wxT(".rep");
				wxString file = ( dir + replayName ).GetFullPath();
				Console.WriteLn(Color_StrongGreen, wxT("Saving replay to ") + file);
				_replay->SaveToFile(file);
			}
			catch(std::exception& e)
			{
				Console.Error("REPLAY: %s", e.what());
				Stop();
			}
			_replay.reset();
		}
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
			if(_replay)
				_replay->SyncState(*state);
			if(!_session->join(ep, *state,
				boost::bind(&NetplayPlugin::CheckSyncStates, this, _1, _2), timeout))
				return false;
			INetplayDialog* dialog = INetplayDialog::GetInstance();
			ExecuteOnMainThread([&]() {
				dialog->OnConnectionEstablished(_session->delay());
			});
			std::string player_name;
			auto ep = _session->endpoints()[0];
			player_name = _session->username(ep);
			if(!player_name.length())
			{
				std::stringstream ss;
				ss << ep.address().to_string() << ":" << ep.port();
				player_name = ss.str();
			}

			wxString wxName = wxString(player_name.c_str(), wxConvUTF8);
			ExecuteOnMainThread([&]() {
				Console.WriteLn(Color_StrongGreen, wxT("NETPLAY: Connected to ") + wxName +
					wxT(". Starting memory card synchronization."));
			});
			ExecuteOnMainThread([&]() {
				dialog->SetStatus(wxT("Connected to ") + wxName);
			});

			{
				wxString myName = wxString(_session->username().c_str(), wxConvUTF8);
				if(!myName.Len())
					myName = wxT("Me");
				_game_name = wxDateTime::Now().Format(wxT("%Y.%m.%d_%Hh%Mm_")) + wxName + wxT("_vs_") + myName;
			}

			int delay = dialog->WaitForConfirmation();
			if(delay <= 0)
				return false;

			ExecuteOnMainThread([&]() {
				dialog->SetStatus(wxT("Memory card synchronization..."));
			});

			mcd_backup = Utilities::ReadMCD(0,0);

			shoryu::msec timeout_timestamp = shoryu::time_ms() + 30000;
			size_t mcd_size = Utilities::GetMCDSize(0,0);
			Utilities::block_type compressed_mcd(mcd_size);
			size_t pos = 0;
			while(true)
			{
				if(_session->end_session_request())
					return false;
				shoryu::message_data data;
				if(!_session->get_data(1-_session->side(), data, 50))
				{
					if(timeout_timestamp < shoryu::time_ms())
					{
						ExecuteOnMainThread([&]() {
							Console.Error("NETPLAY: Timeout while synchonizing memory cards.");
						});
						RequestStop();
						return false;
					}
				}
				else
				{
					if(data.data_length != 9 || memcmp(data.p.get(), "BLOCK_END", 9) != 0)
					{
						std::copy(data.p.get(), data.p.get() + data.data_length, compressed_mcd.data()+pos);
						pos += data.data_length;
					}
					else
					{
						compressed_mcd.resize(pos);
						Utilities::block_type uncompressed_mcd(mcd_size);

						if(!Utilities::Uncompress(compressed_mcd, uncompressed_mcd))
						{
							ExecuteOnMainThread([&]() {
								Console.Error("NETPLAY: Unable to decompress MCD buffer.");
							});
							RequestStop();
							return false;
						}
						if(uncompressed_mcd.size() != mcd_size)
						{
							ExecuteOnMainThread([&]() {
								Console.Error("NETPLAY: Invalid MCD received from host.");
							});
							RequestStop();
							return false;
						}
						Utilities::WriteMCD(0,0,uncompressed_mcd);
						if(_replay)
							_replay->Data(uncompressed_mcd);
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
			/*
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
			*/
			_connectionEstablished = true;
			return true;
		}
		else
			RequestStop();
		return false;
	}
	virtual bool Host(int timeout)
	{
		auto state = GetSyncState();
		if(state)
		{
			if(_replay)
				_replay->SyncState(*state);
			if(!_session->create(2, *state,
				boost::bind(&NetplayPlugin::CheckSyncStates, this, _1, _2), timeout))
				return false;
			INetplayDialog* dialog = INetplayDialog::GetInstance();
			ExecuteOnMainThread([&]() {
				dialog->OnConnectionEstablished(_session->delay());
			});
			std::string player_name;
			auto ep = _session->endpoints()[0];
			player_name = _session->username(ep);
			if(!player_name.length())
			{
				std::stringstream ss;
				ss << ep.address().to_string() << ":" << ep.port();
				player_name = ss.str();
			}
			wxString wxName = wxString(player_name.c_str(), wxConvUTF8);
			ExecuteOnMainThread([&]() {
				Console.WriteLn(Color_StrongGreen, wxT("NETPLAY: Connection from ") + wxName +
					wxT(". Starting memory card synchronization."));
			});
			ExecuteOnMainThread([&]() {
				dialog->SetStatus(wxT("Connection from ") + wxName);
			});

			{
				wxString myName = wxString(_session->username().c_str(), wxConvUTF8);
				if(!myName.Len())
					myName = wxT("Me");
				_game_name = wxDateTime::Now().Format(wxT("%Y.%m.%d_%Hh%Mm_")) + myName + wxT("_vs_") + wxName;
			}

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

			auto uncompressed_mcd = Utilities::ReadMCD(0,0);
			if(_replay)
				_replay->Data(uncompressed_mcd);
			size_t mcd_size = Utilities::GetMCDSize(0,0);
			Utilities::block_type compressed_mcd(mcd_size);
			if(g_Conf->Net.ReadonlyMemcard)
				mcd_backup = uncompressed_mcd;
			if(!Utilities::Compress(uncompressed_mcd, compressed_mcd))
			{
				ExecuteOnMainThread([&]() {
					Console.Error("NETPLAY: Unable to compress MCD buffer.");
				});
				RequestStop();
				return false;
			}
			size_t blockSize = 128;
			for(size_t i = 0; i < compressed_mcd.size(); i+=blockSize)
			{
				shoryu::message_data data;
				data.data_length = compressed_mcd.size() - i;
				if(data.data_length > blockSize)
					data.data_length = blockSize;
				data.p.reset(new char[data.data_length]);
				std::copy(compressed_mcd.data()+i, compressed_mcd.data()+i+data.data_length, data.p.get());
				_session->queue_data(data);
			}
			shoryu::message_data data;
			data.data_length = 9;
			data.p.reset(new char[data.data_length]);
			char* blockEndMsg = "BLOCK_END";
			std::copy(blockEndMsg, blockEndMsg + data.data_length, data.p.get());
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
				//Console.WriteLn("Sending %d packets", n);
				if(timeout_timestamp < shoryu::time_ms())
				{
					ExecuteOnMainThread([&]() {
						Console.Error("NETPLAY: Timeout while synchonizing memory cards.");
					});
					RequestStop();
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
			RequestStop();
			_session->shutdown();
			_session->unbind();
		}
	}
	void RequestStop()
	{
		if(_session && _session->state() == shoryu::Ready)
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
	}
	void Stop()
	{
		ExecuteOnMainThread([&]() {
			INetplayDialog* dialog = INetplayDialog::GetInstance();
			if(dialog->IsShown())
				dialog->Close();
		});
		if(_thread)
			_thread->join();
		CoreThread.Reset();
		UI_EnableEverything();
		_sessionEnded = true;
	}
	void NextFrame()
	{
		if(!_connectionEstablished)
		{
			if(_thread->timed_join(boost::posix_time::milliseconds(3000)))
			{
				if(_session->state() != shoryu::Ready)
				{
					Console.Error("NETPLAY: Unable to establish connection.");
					Stop();
					return;
				}
				if(!_connectionEstablished)
				{
					Console.Error("NETPLAY: Interrupted.");
					Stop();
					return;
				}
			}
		}
		if(_thread && _connectionEstablished)
		{
			_thread.reset();
			ExecuteOnMainThread([&]() {
				INetplayDialog* dialog = INetplayDialog::GetInstance();
				if(dialog->IsShown())
					dialog->Close();
			});
			Console.WriteLn(Color_StrongGreen, "NETPLAY: Delay %d. Starting netplay.", _session->delay());
		}

		myFrame = Message();
		_session->next_frame();
		if(_session->last_error().length())
		{
			// 'The operation completed successfully' is a really strange error.
			// Research it!
			// Console.Error("NETPLAY: %s.", _session->last_error().c_str());
			// _session->last_error("");
		}
	}
	void AcceptInput(int side)
	{
		if(_connectionEstablished && side == 0)
		{
			try
			{
				_session->set(myFrame);
			}
			catch(std::exception& e)
			{
				Console.Error("NETPLAY: %s. Interrupting sessions.", e.what());
				Stop();
			}
		}
		if(_replay)
		{
			Message f;
			if(synchronized)
				_session->get(side, f, 0);

			_replay->Write(side, f);
		}
	}
	u8 HandleIO(int side, int index, u8 value)
	{
		if(_session && _session->end_session_request() && !_sessionEnded)
		{
			Console.Warning("NETPLAY: Session ended.");
			Stop();
		}
		if(_sessionEnded)
			return value;

		Message f;
		if(side == 0)
		{
			if(_connectionEstablished)
			{
				if(!synchronized)
				{
					synchronized = _session->first_received_frame() > 0
						&& _session->frame() > _session->first_received_frame()
						&& _session->frame() <= _session->last_received_frame();
				}
			}
			if(!synchronized)
			{
				if(_session->frame() > _session->last_received_frame())
						shoryu::sleep(50);
			}
			else
				myFrame.input[index] = value;
		}
		try
		{
			if(synchronized)
			{
				auto timeout = shoryu::time_ms() + 10000;
				while(!_session->get(side, f, _session->delay()*17))
				{
					_session->send();
					if(_session->end_session_request())
						break;
					if(timeout <= shoryu::time_ms())
					{
						Console.Warning("NETPLAY: Timeout.");
						Stop();
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
			Console.Error("NETPLAY: %s", e.what());
			Stop();
		}
		value = f.input[index];
		return value;
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
			ExecuteOnMainThread([&]() {
				Console.Error("NETPLAY: Bios version mismatch.");
			});
			return false;
		}
		if(memcmp(s1.discSerial, s2.discSerial, sizeof(s1.discSerial)))
		{
			std::stringstream ss;
			ss << "NETPLAY: You are trying to boot different games ("<<s1.discSerial<<" != "<<s2.discSerial<<").";
			ExecuteOnMainThread([&]() {
				Console.Error(ss.str().c_str());
			});
			return false;
		}
		return true;
	}
	
	bool _sessionEnded;
	bool _connectionEstablished;

	wxString _game_name;
	Message myFrame;
	Pcsx2Config EmuOptionsBackup;
	bool Mcd1EnabledBackup;
	bool Mcd2EnabledBackup;
	bool EnableGameFixesBackup;
	AppConfig::ConsoleLogOptions ProgLogBoxBackup;
	bool replaced;
	bool synchronized;
	Utilities::block_type mcd_backup;
	boost::shared_ptr<Replay> _replay;


	boost::shared_ptr<EmulatorSyncState> GetSyncState()
	{
		boost::shared_ptr<EmulatorSyncState> syncState(new EmulatorSyncState());

		cdvdReloadElfInfo();
		if(DiscSerial.Len() == 0)
		{
			ExecuteOnMainThread([&]() {
				Console.Error("NETPLAY: Unable to read disc serial.");
			});
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
			ExecuteOnMainThread([&]() {
				Console.Error("NETPLAY: Unable to read BIOS name.");
			});
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
