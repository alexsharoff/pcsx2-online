#include "PrecompiledHeader.h"
#include "NetplayDialog.h"


void NetplayDialog::onCheckBoxClicked( wxCommandEvent& event )
{
	textBoxHostIP->Enable(!checkBoxHostGame->IsChecked());
	textBoxHostPort->Enable(!checkBoxHostGame->IsChecked());
}
void NetplayDialog::buttonCancel_Clicked( wxCommandEvent& event )
{
	EndModal(wxID_CANCEL);
}
void NetplayDialog::buttonOK_Clicked( wxCommandEvent& event )
{
	UpdateOptions();
	EndModal(wxID_OK);
}

void NetplayDialog::UpdateOptions()
{
	long port;
	if( !textBoxPort->GetValue().ToLong(&port, 10))
		_options.LocalPort = 7500;
	else
		_options.LocalPort = port;

	if(!textBoxHostPort->GetValue().ToLong(&port, 10))
		_options.HostPort = 7500;
	else
		_options.HostPort = port;

	_options.HostAddress = textBoxHostIP->GetValue();
	_options.Mode = checkBoxHostGame->IsChecked() ? HostMode : ConnectMode;

	_options.SanityCheck();
	UpdateFromOptions();
}

void NetplayDialog::UpdateFromOptions()
{
	textBoxPort->SetValue(
		wxString::Format(wxT("%d"), (int)_options.LocalPort));
	textBoxHostPort->SetValue(
		wxString::Format(wxT("%d"), (int)_options.HostPort));
	textBoxHostIP->SetValue(_options.HostAddress);
	checkBoxHostGame->SetValue(_options.Mode != ConnectMode);

	onCheckBoxClicked(wxCommandEvent());
}

NetplayDialog::NetplayDialog(NetplaySettings& options, wxWindow* parent) 
:NetplayDialogBase(parent), _options(options)
{
	UpdateFromOptions();
	Fit();
}
