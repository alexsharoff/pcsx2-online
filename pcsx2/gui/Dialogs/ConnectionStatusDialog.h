#pragma once

#include "ConnectionStatusDialogBase.h"
#include "Netplay.h"
#include "AppConfig.h"
#include "pthread.h"

class ConnectionStatusDialog : public ConnectionStatusDialogBase
{
	static void* ConnectionWorker(void* param);
	void ButtonCancelClicked( wxCommandEvent& event );
	void ButtonOKClicked( wxCommandEvent& event );
	Netplay* _net;
	AppConfig::NetOptions* _options;
	pthread_t _connectionThread;
	bool _confirmation;
public:
	ConnectionStatusDialog(Netplay* net, AppConfig::NetOptions* options, wxWindow* parent);
};