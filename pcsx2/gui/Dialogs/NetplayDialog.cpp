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
		m_options->MyPort = 7500;
	else
		m_options->MyPort = port;

	if(!textBoxHostPort->GetValue().ToLong(&port, 10))
		m_options->RemotePort = 7500;
	else
		m_options->RemotePort = port;

	m_options->RemoteIp = textBoxHostIP->GetValue();
	m_options->Connect = !checkBoxHostGame->IsChecked();

	m_options->SanityCheck();
	UpdateFromOptions();
}

void NetplayDialog::UpdateFromOptions()
{
	textBoxPort->SetValue(
		wxString::Format(wxT("%d"), (int)m_options->MyPort));
	textBoxHostPort->SetValue(
		wxString::Format(wxT("%d"), (int)m_options->RemotePort));
	textBoxHostIP->SetValue(m_options->RemoteIp);
	checkBoxHostGame->SetValue(!m_options->Connect);

	onCheckBoxClicked(wxCommandEvent());
}

NetplayDialog::NetplayDialog(AppConfig::NetOptions* options, wxWindow* parent) 
:NetplayDialogBase(parent), m_options(options)
{
	UpdateFromOptions();
	Fit();
}