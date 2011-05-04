///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Sep  8 2010)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "PrecompiledHeader.h"
#include "ConnectionStatusDialogBase.h"

///////////////////////////////////////////////////////////////////////////

ConnectionStatusDialogBase::ConnectionStatusDialogBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	wxFlexGridSizer* fgSizer2;
	fgSizer2 = new wxFlexGridSizer( 1, 1, 0, 0 );
	fgSizer2->SetFlexibleDirection( wxBOTH );
	fgSizer2->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	_statusText = new wxStaticText( this, wxID_ANY, wxT("Status..."), wxPoint( 0,0 ), wxDefaultSize, 0 );
	_statusText->Wrap( -1 );
	fgSizer2->Add( _statusText, 0, wxALIGN_LEFT|wxALIGN_TOP|wxALL|wxLEFT|wxTOP, 5 );
	
	_dialogButtons = new wxStdDialogButtonSizer();
	_dialogButtonsOK = new wxButton( this, wxID_OK );
	_dialogButtons->AddButton( _dialogButtonsOK );
	_dialogButtonsCancel = new wxButton( this, wxID_CANCEL );
	_dialogButtons->AddButton( _dialogButtonsCancel );
	_dialogButtons->Realize();
	fgSizer2->Add( _dialogButtons, 1, wxALIGN_BOTTOM|wxALIGN_RIGHT|wxBOTTOM|wxEXPAND|wxRIGHT, 5 );
	
	this->SetSizer( fgSizer2 );
	this->Layout();
	
	this->Centre( wxBOTH );
	
	// Connect Events
	_dialogButtonsCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConnectionStatusDialogBase::ButtonCancelClicked ), NULL, this );
	_dialogButtonsOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConnectionStatusDialogBase::ButtonOKClicked ), NULL, this );
}

ConnectionStatusDialogBase::~ConnectionStatusDialogBase()
{
	// Disconnect Events
	_dialogButtonsCancel->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConnectionStatusDialogBase::ButtonCancelClicked ), NULL, this );
	_dialogButtonsOK->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConnectionStatusDialogBase::ButtonOKClicked ), NULL, this );
	
}
