#pragma once

#include "PrecompiledHeader.h"
#include "NetplayDialogBase.h"
#include "AppConfig.h"

class NetplayDialog : public NetplayDialogBase
{
	void onCheckBoxClicked( wxCommandEvent& event );
	void buttonCancel_Clicked( wxCommandEvent& event );
	void buttonOK_Clicked( wxCommandEvent& event );
	AppConfig::NetOptions& _options;
	void UpdateFromOptions();
	void UpdateOptions();
public:
	NetplayDialog(AppConfig::NetOptions& options, wxWindow* parent);
};
