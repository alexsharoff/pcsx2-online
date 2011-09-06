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
		_options.MyPort = 7500;
	else
		_options.MyPort = port;

	if(!textBoxHostPort->GetValue().ToLong(&port, 10))
		_options.RemotePort = 7500;
	else
		_options.RemotePort = port;

	_options.RemoteIp = textBoxHostIP->GetValue();
	_options.Connect = !checkBoxHostGame->IsChecked();

	_options.SanityCheck();
	UpdateFromOptions();
}

void NetplayDialog::UpdateFromOptions()
{
	textBoxPort->SetValue(
		wxString::Format(wxT("%d"), (int)_options.MyPort));
	textBoxHostPort->SetValue(
		wxString::Format(wxT("%d"), (int)_options.RemotePort));
	textBoxHostIP->SetValue(_options.RemoteIp);
	checkBoxHostGame->SetValue(!_options.Connect);

	onCheckBoxClicked(wxCommandEvent());
}

NetplayDialog::NetplayDialog(AppConfig::NetOptions& options, wxWindow* parent) 
:NetplayDialogBase(parent), _options(options)
{
	UpdateFromOptions();
	Fit();
}