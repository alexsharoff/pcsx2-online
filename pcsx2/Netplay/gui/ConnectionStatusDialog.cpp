#include "PrecompiledHeader.h"
#include "ConnectionStatusDialog.h"
#include "pthread.h"

void ConnectionStatusDialog::ButtonCancelClicked( wxCommandEvent& event )
{
	_cancelled = true;
	INetplayPlugin::GetInstance().EndSession();
	pthread_join(_connectionThread, 0);
	EndModal(wxID_CANCEL);
}

void ConnectionStatusDialog::ButtonOKClicked( wxCommandEvent& event )
{
	_dialogButtonsOK->Disable();
	_confirmation = true;
	_statusText->SetLabel(wxT("Waiting for confirmation..."));
}

void* ConnectionStatusDialog::ConnectionWorker(void* param)
{
	ConnectionStatusDialog* dialog = (ConnectionStatusDialog*)param;
	if(!INetplayPlugin::GetInstance().BindPort(dialog->_options.MyPort))
	{
		dialog->_statusText->SetLabel(wxString::Format(wxT("Selected port is already in use.")));
		return 0;
	}
	if(dialog->_options.Connect)
	{
		dialog->_statusText->SetLabel(wxT("Connecting..."));

		if(!dialog->_netplay.Connect(
			dialog->_options.RemoteIp, dialog->_options.RemotePort, 500))
		{
			if(!dialog->_cancelled)
			{
				dialog->_statusText->SetLabel(wxString::Format(wxT("Unknown error.")));
				return 0;
			}
		}
	}
	else
	{
		dialog->_statusText->SetLabel(wxT("Hosting..."));
		if(!dialog->_netplay.Host())
		{
			if(!dialog->_cancelled)
			{
				dialog->_statusText->SetLabel(wxString::Format(wxT("Unknown error.")));
				return 0;
			}
		}
	}
	if(!dialog->_cancelled)
	{
		dialog->_statusText->SetLabel(
			wxString::Format(wxT("Delay %d"), dialog->_netplay.GetConnectionEstimate()));
		dialog->_dialogButtonsOK->Enable(true);
		dialog->EndModal(wxID_OK);
	}
	return 0;
}

ConnectionStatusDialog::ConnectionStatusDialog(INetplayPlugin& netplay,
	AppConfig::NetOptions& options,
	wxWindow* parent) :
_options (options), _netplay(netplay), ConnectionStatusDialogBase(parent)
{ 
	_dialogButtonsOK->Enable(false);
	pthread_create(&_connectionThread, 0, ConnectionStatusDialog::ConnectionWorker, this);
	_confirmation = false;
	_cancelled = false;
}
