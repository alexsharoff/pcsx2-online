#pragma once
#include "App.h"
#include "EmulatorState.h"
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include "PS2Edefs.h"

class Utilities
{
public:
	typedef std::vector<u8> block_type;

	// TODO: use rvalue references here
	static block_type ReadMCD(uint port, uint slot);
	static void WriteMCD(uint port, uint slot, const block_type& block);
	static bool Compress(const Utilities::block_type& uncompressed,
		Utilities::block_type& compressed);
	static bool Uncompress(const Utilities::block_type& compressed,
		Utilities::block_type& uncompressed);
	static size_t GetMCDSize(uint port, uint slot);
	static boost::shared_ptr<EmulatorSyncState> GetSyncState();
	static wxString GetDiscNameById(const wxString& id);
	static wxString GetCurrentDiscId();
	static wxString GetCurrentDiscName();

	static void ExecuteOnMainThread(const std::function<void()>& evt);

	static void SetKeyHandler(const keyEvent& key, const std::function<void()>& handler);
	static void DispatchKeyHandler(const keyEvent& key);
	
	static void SaveSettings();
	static void ResetSettingsToSafeDefaults();
	static void RestoreSettings();
private:
	static std::function<void()> _dispatch_event;
	static void DispatchEvent();
	static boost::unordered_map<keyEvent, std::function<void()> > _keyMappings;
	static std::auto_ptr<AppConfig> _settingsBackup;
};
