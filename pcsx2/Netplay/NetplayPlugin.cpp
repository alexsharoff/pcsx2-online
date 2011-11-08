#include "PrecompiledHeader.h"

#include "AppConfig.h"
#include <wx/stdpaths.h>
#include <iostream>
#include <fstream>
#include "Netplay/NetplayPlugin.h"
#include "Netplay/INetplayDialog.h"

#include "shoryu/session.h"
#include "Message.h"
#include "Replay.h"
#include "NetplaySettings.h"
#include "Utilities.h"


//#define CONNECTION_TEST

class NetplayPlugin : public INetplayPlugin
{
	typedef shoryu::session<Message, EmulatorSyncState> session_type;
	boost::shared_ptr<session_type> _session;
	boost::shared_ptr<boost::thread> _thread;
public:
	NetplayPlugin() : _is_init(false) {}
	virtual void Open()
	{
		NetplaySettings& settings = g_Conf->Net;
		if( settings.LocalPort <= 0 || settings.LocalPort > 65535 )
		{
			Stop();
			Console.Error("NETPLAY: Invalid port: %u.", settings.LocalPort);
			return;
		}
		if( settings.Mode == ConnectMode && (settings.HostPort <= 0 || settings.HostPort > 65535) )
		{
			Stop();
			Console.Error("NETPLAY: Invalid port: %u.", settings.HostPort);
			return;
		}
		if( settings.Mode == ConnectMode && settings.HostAddress.Len() == 0 )
		{
			Stop();
			Console.Error("NETPLAY: Invalid hostname.");
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
			_state = None;
			_session->username(std::string((const char*)settings.Username.mb_str(wxConvUTF8)));

			if(g_Conf->Net.SaveReplay)
			{
				_replay.reset(new Replay());
				_replay->Mode(Recording);
			}
			_game_name.clear();
			std::function<bool()> connection_func;
			if(settings.Mode == ConnectMode)
				connection_func = [this, settings]() { return Connect(settings.HostAddress,settings.HostPort, 0); };
			else
				connection_func = [this]() { return Host(0); };

			_thread.reset(new boost::thread([this, connection_func]() {
				_state = connection_func() ? Ready : Cancelled;
				_session->send();
				INetplayDialog* dialog = INetplayDialog::GetInstance();
				if(dialog->IsShown())
				{
					Utilities::ExecuteOnMainThread([&]() {
						dialog->Close();
					});
				}
			}));
		}
		else
		{
			Stop();
			Console.Error("NETPLAY: Unable to bind port %u.", settings.LocalPort);
		}
	}
	bool IsInit()
	{
		return _is_init;
	}
	void Init()
	{
		_is_init = true;
		Utilities::SaveSettings();
		Utilities::ResetSettingsToSafeDefaults();
	}
	void Close()
	{
		_is_init = false;
		EndSession();
		Utilities::RestoreSettings();
		if(_mcd_backup.size())
		{
			Utilities::WriteMCD(0,0,_mcd_backup);
			_mcd_backup.clear();
		}
		if(_replay)
		{
			if(_state == Running)
			{
				try
				{
					wxDirName dir = (wxDirName)wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
					dir = dir.Combine(wxDirName("replays"));
					wxString replayName = _game_name + wxT(".rep");
					replayName.Replace(wxT("<"),wxT("-"));
					replayName.Replace(wxT(">"),wxT("-"));
					replayName.Replace(wxT(":"),wxT("-"));
					replayName.Replace(wxT("\""),wxT("-"));
					replayName.Replace(wxT("/"),wxT("-"));
					replayName.Replace(wxT("\\"),wxT("-"));
					replayName.Replace(wxT("|"),wxT("-"));
					replayName.Replace(wxT("?"),wxT("-"));
					replayName.Replace(wxT("*"),wxT("-"));
					wxString file = ( dir + replayName ).GetFullPath();
					Console.WriteLn(Color_StrongGreen, wxT("Saving replay to ") + file);
					_replay->SaveToFile(file);
				}
				catch(std::exception& e)
				{
					Stop();
					Console.Error("REPLAY: %s", e.what());
				}
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
		int try_count = 15;
		while(try_count-->0)
		{
			if(Utilities::IsSyncStateReady())
				break;
			shoryu::sleep(500);
		}
		shoryu::endpoint ep = shoryu::resolve_hostname(std::string(ip.ToAscii().data()));
		ep.port(port);
		auto state = Utilities::GetSyncState();
		if(state && try_count)
		{
			if(_replay)
				_replay->SyncState(*state);
			if(!_session->join(ep, *state,
				boost::bind(&NetplayPlugin::CheckSyncStates, this, _1, _2), timeout))
			{
				RequestStop();
				return false;
			}
			INetplayDialog* dialog = INetplayDialog::GetInstance();
			Utilities::ExecuteOnMainThread([&]() {
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
			Utilities::ExecuteOnMainThread([&]() {
				dialog->SetStatus(wxT("Connected to ") + wxName);
			});
			{
				wxString myName = wxString(_session->username().c_str(), wxConvUTF8);
				if(!myName.Len())
					myName = wxT("Me");

				_game_name = wxDateTime::Now().Format(wxT("[%Y.%m.%d %H-%M] "))  + wxT(" [") + Utilities::GetCurrentDiscName() + wxT("] ") + wxName + wxT(" vs ") + myName;
			}

			int delay = dialog->WaitForConfirmation();
			if(delay <= 0)
			{
				RequestStop();
				return false;
			}

			Utilities::ExecuteOnMainThread([&]() {
				dialog->SetStatus(wxT("Memory card synchronization..."));
			});

			_mcd_backup = Utilities::ReadMCD(0,0);

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
						RequestStop();
						Utilities::ExecuteOnMainThread([&]() {
							Console.Error("NETPLAY: Timeout while synchonizing memory cards.");
						});
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
							RequestStop();
							Utilities::ExecuteOnMainThread([&]() {
								Console.Error("NETPLAY: Unable to decompress MCD buffer.");
							});
							return false;
						}
						if(uncompressed_mcd.size() != mcd_size)
						{
							RequestStop();
							Utilities::ExecuteOnMainThread([&]() {
								Console.Error("NETPLAY: Invalid MCD received from host.");
							});
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
					Utilities::ExecuteOnMainThread([&]() {
						if(dialog->IsShown())
							dialog->SetInputDelay(_session->delay());
					});
				}
			}
			return true;
		}
		else
		{
			RequestStop();
			return false;
		}
	}
	virtual bool Host(int timeout)
	{
		int try_count = 15;
		while(try_count-->0)
		{
			if(Utilities::IsSyncStateReady())
				break;
			shoryu::sleep(500);
		}
		auto state = Utilities::GetSyncState();
		if(state && try_count)
		{
			if(_replay)
				_replay->SyncState(*state);
			if(!_session->create(2, *state,
				boost::bind(&NetplayPlugin::CheckSyncStates, this, _1, _2), timeout))
			{
				RequestStop();
				return false;
			}
			INetplayDialog* dialog = INetplayDialog::GetInstance();
			Utilities::ExecuteOnMainThread([&]() {
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
			Utilities::ExecuteOnMainThread([&]() {
				dialog->SetStatus(wxT("Connection from ") + wxName);
			});
			{
				wxString myName = wxString(_session->username().c_str(), wxConvUTF8);
				if(!myName.Len())
					myName = wxT("Me");
				_game_name = wxDateTime::Now().Format(wxT("[%Y.%m.%d %H-%M] "))  + wxT(" [") + Utilities::GetCurrentDiscName() + wxT("] ") + myName + wxT(" vs ") + wxName;
			}

			int delay = dialog->WaitForConfirmation();
			if(delay <= 0)
			{
				RequestStop();
				return false;
			}
			if(delay != _session->delay())
			{
				_session->delay(delay);
				_session->reannounce_delay();
			}
			Utilities::ExecuteOnMainThread([&]() {
				dialog->SetStatus(wxT("Memory card synchronization..."));
			});
			auto uncompressed_mcd = Utilities::ReadMCD(0,0);
			if(_replay)
				_replay->Data(uncompressed_mcd);
			size_t mcd_size = Utilities::GetMCDSize(0,0);
			Utilities::block_type compressed_mcd(mcd_size);
			if(g_Conf->Net.ReadonlyMemcard)
				_mcd_backup = uncompressed_mcd;
			if(!Utilities::Compress(uncompressed_mcd, compressed_mcd))
			{
				RequestStop();
				Utilities::ExecuteOnMainThread([&]() {
					Console.Error("NETPLAY: Unable to compress MCD buffer.");
				});
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
				if(timeout_timestamp < shoryu::time_ms())
				{
					RequestStop();
					Utilities::ExecuteOnMainThread([&]() {
						Console.Error("NETPLAY: Timeout while synchonizing memory cards.");
					});
					return false;
				}
			}
			return true;
		}
		else
		{
			RequestStop();
			return false;
		}
	}
	void EndSession()
	{
		INetplayDialog* dialog = INetplayDialog::GetInstance();
		if(dialog->IsShown())
		{
			Utilities::ExecuteOnMainThread([&]() {
				dialog->Close();
			});
		}
		if(_session)
		{
			RequestStop();
			_session->shutdown();
			_session->unbind();
			_session.reset();
		}
	}
	void RequestStop()
	{
		if(_session)
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
		EndSession();
		Utilities::ExecuteOnMainThread([&]() {
			CoreThread.Reset();
			UI_EnableEverything();
		});
		if(_thread)
			_thread->join();
	}
	void NextFrame()
	{
		if(!_session)
			return;
		_my_frame = Message();
		_session->next_frame();
		if(_state == Ready)
		{
			if(_thread) _thread.reset();
			_state = Running;
		}
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
		if(!_session)
			return;
		try
		{
			_session->set(_my_frame);
		}
		catch(std::exception& e)
		{
			Stop();
			Console.Error("NETPLAY: %s. Interrupting session.", e.what());
		}
		if(_replay)
		{
			Message f;
			_session->get(side, f, 0);
			_replay->Write(side, f);
		}
	}
	u8 HandleIO(int side, int index, u8 value)
	{
		if(_session)
		{
			while(_state == None) shoryu::sleep(35);

			if(_session->end_session_request() || _state == Cancelled)
			{
				Stop();
				Console.Warning("NETPLAY: Session ended on frame %d.", _session->frame());
			}
		}

		if(!_session) return value;

		Message frame;
		if(side == 0)
			_my_frame.input[index] = value;
		auto timeout = shoryu::time_ms() + 10000;
		try
		{
			while(!_session->get(side, frame, _session->delay()*17))
			{
				_session->send();
				if(_session->end_session_request())
					break;
				if(timeout <= shoryu::time_ms())
				{
					Stop();
					Console.Error("NETPLAY: Timeout on frame %d.", _session->frame());
					break;
				}
#ifdef CONNECTION_TEST
				shoryu::sleep(500);
#endif
			}
		}
		catch(std::exception& e)
		{
			Stop();
			Console.Error("NETPLAY: %s", e.what());
		}
		value = frame.input[index];
		return value;
	}
protected:
	bool CheckSyncStates(const EmulatorSyncState& s1, const EmulatorSyncState& s2)
	{
		if(memcmp(s1.biosVersion, s2.biosVersion, sizeof(s1.biosVersion)))
		{
			Utilities::ExecuteOnMainThread([&]() {
				Console.Error("NETPLAY: Bios version mismatch.");
			});
			return false;
		}
		if(memcmp(s1.discId, s2.discId, sizeof(s1.discId)))
		{
			size_t s1discIdLen = sizeof(s1.discId);
			size_t s2discIdLen = sizeof(s2.discId);
			for(size_t i = 0; i < s1discIdLen; i++)
			{
				if(s1.discId[i] == 0)
				{
					s1discIdLen = i;
					break;
				}
			}
			for(size_t i = 0; i < s2discIdLen; i++)
			{
				if(s2.discId[i] == 0)
				{
					s2discIdLen = i;
					break;
				}
			}
			wxString s1discId(s1.discId, wxConvUTF8, s1discIdLen);
			wxString s2discId(s2.discId, wxConvUTF8, s2discIdLen);

			Utilities::ExecuteOnMainThread([&]() {
				Console.Error(
					wxString(wxT("NETPLAY: You are trying to boot different games: ")) + 
					Utilities::GetDiscNameById(s1discId) + wxString(wxT(" and ")) + Utilities::GetDiscNameById(s2discId));
			});
			return false;
		}
		return true;
	}
	
	enum NetplayState
	{
		None,
		Cancelled,
		Ready,
		Running
	} _state;
	bool _is_init;
	wxString _game_name;
	Message _my_frame;
	Utilities::block_type _mcd_backup;
	boost::shared_ptr<Replay> _replay;
};

INetplayPlugin* INetplayPlugin::instance = 0;

INetplayPlugin& INetplayPlugin::GetInstance()
{
	if(!instance)
		instance = new NetplayPlugin();
	return *instance;
}
