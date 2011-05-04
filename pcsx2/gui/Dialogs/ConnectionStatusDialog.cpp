#include "PrecompiledHeader.h"
#include "ConnectionStatusDialog.h"
#include "Net.h"

void ConnectionStatusDialog::ButtonCancelClicked( wxCommandEvent& event )
{
	_net->endSession();
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
	if(!dialog->_net->start(dialog->_options->MyPort))
	{
		dialog->_statusText->SetLabel(wxString::Format(wxT("Port is already in use.")));
		return 0;
	}
	bool connected = false;

	if(dialog->_options->Connect)
	{
		dialog->_statusText->SetLabel(wxT("Connecting..."));

		while(dialog->_net->isActive())
		{
			if(!connected)
			{
				int rez = dialog->_net->connect(dialog->_options->RemoteIp.ToAscii().data(), 
					dialog->_options->RemotePort, 500);
				if(rez == 1)
				{
					dialog->_statusText->SetLabel(
						wxString::Format(wxT("Ping = %d ms"), dialog->_net->getPeers()[0]->stats.rtt()));
					dialog->_dialogButtonsOK->Enable(true);
					connected = true;
				}
				if(rez == -1)
				{
					dialog->_statusText->SetLabel(wxString::Format(wxT("Unknown error.")));
					return 0;
				}
			}
			else
			{
				if(dialog->_confirmation)
				{
					Endpoint ep = dialog->_net->getPeers()[0]->remote_ep;
					dialog->_net->send(ep, "OK", 2);
					char ok[3];
					dialog->_net->recv(ep, ok, 2, 300);
					ok[2] = 0;
					if(strcmp(ok,"OK") == 0)
					{
						dialog->_net->send(ep, "OK", 2);
						dialog->EndModal(wxID_OK);
						return 0;
					}
				}
				else
					wxUsleep(300);
			}
		}
	}
	else
	{
		dialog->_statusText->SetLabel(wxT("Hosting..."));

		while(dialog->_net->isActive())
		{
			if(!connected)
			{
				int rez = dialog->_net->host(500);
				if(rez == 1)
				{
					dialog->_statusText->SetLabel(
						wxString::Format(wxT("Ping = %d ms"), dialog->_net->getPeers()[0]->stats.rtt()));

					dialog->_dialogButtonsOK->Enable(true);
					connected = true;
				}

				if(rez == -1)
				{
					dialog->_statusText->SetLabel(wxString::Format(wxT("Unknown error.")));
					return 0;
				}
			}
			else
			{
				if(dialog->_confirmation)
				{
					Endpoint ep = dialog->_net->getPeers()[0]->remote_ep;
					dialog->_net->send(ep, "OK", 2);
					char ok[3];
					dialog->_net->recv(ep, ok, 2, 300);
					ok[2] = 0;
					if(strcmp(ok,"OK") == 0)
					{
						dialog->EndModal(wxID_OK);
						return 0;
					}
				}
				else
					wxUsleep(300);
			}
		}
	}
	return 0;
}

ConnectionStatusDialog::ConnectionStatusDialog(Netplay* net, 
											   AppConfig::NetOptions* options, 
											   wxWindow* parent) :
_options (options), _net(net), ConnectionStatusDialogBase(parent)
{ 
	_dialogButtonsOK->Enable(false);
	pthread_create(&_connectionThread, 0, ConnectionStatusDialog::ConnectionWorker, this);
	_confirmation = false;
}
