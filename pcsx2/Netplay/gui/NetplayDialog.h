#pragma once

#include "PrecompiledHeader.h"
#include "NetplayDialogBase.h"
#include "AppConfig.h"
#include "Netplay\NetplaySettings.h"

class NetplayDialog : public NetplayDialogBase
{
	void onCheckBoxClicked( wxCommandEvent& event );
	void buttonCancel_Clicked( wxCommandEvent& event );
	void buttonOK_Clicked( wxCommandEvent& event );
	NetplaySettings& _options;
	void UpdateFromOptions();
	void UpdateOptions();
public:
	NetplayDialog(NetplaySettings& options, wxWindow* parent);
};
