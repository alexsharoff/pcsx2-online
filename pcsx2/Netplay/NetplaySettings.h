#pragma once
#include "Utilities\IniInterface.h"

enum NetplayMode : int
{
	ConnectMode,
	HostMode,
	ObserveMode
};

struct NetplaySettings
{
	uint LocalPort;
	NetplayMode Mode;
	uint HostPort;
	wxString HostAddress;
	bool ReadonlyMemcard;
	bool FinetuneDelay;
	
	NetplaySettings();
	void LoadSave( IniInterface& conf );
	void SanityCheck();
};