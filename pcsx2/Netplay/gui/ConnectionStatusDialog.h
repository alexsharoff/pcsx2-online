#pragma once

#include "ConnectionStatusDialogBase.h"
#include "AppConfig.h"
#include "Netplay/NetplayPlugin.h"

class ConnectionStatusDialog : public ConnectionStatusDialogBase
{
	static void* ConnectionWorker(void* param);
	void ButtonCancelClicked( wxCommandEvent& event );
	void ButtonOKClicked( wxCommandEvent& event );
	AppConfig::NetOptions& _options;
	INetplayPlugin& _netplay;
	pthread_t _connectionThread;
	volatile bool _confirmation;
	volatile bool _cancelled;
public:
	ConnectionStatusDialog(INetplayPlugin& netplay, AppConfig::NetOptions& options, wxWindow* parent);
};