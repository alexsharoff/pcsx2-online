#pragma once
#include "Utilities\IniInterface.h"

struct ReplaySettings
{
	bool IsEnabled;

	wxString FilePath;
	ReplaySettings();
};