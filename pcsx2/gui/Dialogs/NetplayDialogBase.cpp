///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Apr 16 2008)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "PrecompiledHeader.h"
#include "NetplayDialogBase.h"

///////////////////////////////////////////////////////////////////////////

NetplayDialogBase::NetplayDialogBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	wxFlexGridSizer* sizer;
	sizer = new wxFlexGridSizer( 2, 2, 0, 0 );
	sizer->SetFlexibleDirection( wxBOTH );
	sizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	labelPort = new wxStaticText( this, wxID_ANY, wxT("Port"), wxDefaultPosition, wxDefaultSize, 0 );
	labelPort->Wrap( -1 );
	sizer->Add( labelPort, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	textBoxPort = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	textBoxPort->SetToolTip( wxT("Your port.") );
	textBoxPort->SetMinSize( wxSize( 150,-1 ) );
	
	sizer->Add( textBoxPort, 0, wxALL|wxEXPAND, 5 );
	
	
	sizer->Add( 0, 0, 1, wxEXPAND, 5 );
	
	checkBoxHostGame = new wxCheckBox( this, wxID_ANY, wxT("Host the game"), wxDefaultPosition, wxDefaultSize, 0 );
	
	checkBoxHostGame->SetToolTip( wxT("Check to host the game.") );
	
	sizer->Add( checkBoxHostGame, 0, wxALL|wxALIGN_RIGHT, 5 );
	
	labelHostIP = new wxStaticText( this, wxID_ANY, wxT("Host IP"), wxDefaultPosition, wxDefaultSize, 0 );
	labelHostIP->Wrap( -1 );
	sizer->Add( labelHostIP, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	textBoxHostIP = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	textBoxHostIP->SetToolTip( wxT("If you aren't hosting, you should specify host's IP here.") );
	textBoxHostIP->SetMinSize( wxSize( 150,-1 ) );
	
	sizer->Add( textBoxHostIP, 0, wxALL|wxEXPAND, 5 );
	
	labelHostPort = new wxStaticText( this, wxID_ANY, wxT("Host port"), wxDefaultPosition, wxDefaultSize, 0 );
	labelHostPort->Wrap( -1 );
	sizer->Add( labelHostPort, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );
	
	textBoxHostPort = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	textBoxHostPort->SetToolTip( wxT("If you aren't hosting, you should specify host's port here.") );
	textBoxHostPort->SetMinSize( wxSize( 150,-1 ) );
	
	sizer->Add( textBoxHostPort, 0, wxALL|wxEXPAND, 5 );
	
	
	sizer->Add( 0, 0, 1, wxEXPAND, 5 );
	
	dialogButtons = new wxStdDialogButtonSizer();
	dialogButtonsOK = new wxButton( this, wxID_OK );
	dialogButtons->AddButton( dialogButtonsOK );
	dialogButtonsCancel = new wxButton( this, wxID_CANCEL );
	dialogButtons->AddButton( dialogButtonsCancel );
	dialogButtons->Realize();
	sizer->Add( dialogButtons, 1, wxALIGN_BOTTOM|wxALIGN_CENTER_HORIZONTAL|wxBOTTOM|wxEXPAND, 5 );
	
	this->SetSizer( sizer );
	this->Layout();
	
	// Connect Events
	checkBoxHostGame->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( NetplayDialogBase::onCheckBoxClicked ), NULL, this );
	dialogButtonsCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( NetplayDialogBase::buttonCancel_Clicked ), NULL, this );
	dialogButtonsOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( NetplayDialogBase::buttonOK_Clicked ), NULL, this );
}

NetplayDialogBase::~NetplayDialogBase()
{
	// Disconnect Events
	checkBoxHostGame->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( NetplayDialogBase::onCheckBoxClicked ), NULL, this );
	dialogButtonsCancel->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( NetplayDialogBase::buttonCancel_Clicked ), NULL, this );
	dialogButtonsOK->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( NetplayDialogBase::buttonOK_Clicked ), NULL, this );
}
