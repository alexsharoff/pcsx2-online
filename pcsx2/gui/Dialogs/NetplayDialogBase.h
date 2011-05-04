///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Apr 16 2008)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#ifndef __NetplayDialogBase__
#define __NetplayDialogBase__

#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class NetplayDialogBase
///////////////////////////////////////////////////////////////////////////////
class NetplayDialogBase : public wxDialog 
{
	private:
	
	protected:
		wxStaticText* labelPort;
		wxTextCtrl* textBoxPort;
		
		wxCheckBox* checkBoxHostGame;
		wxStaticText* labelHostIP;
		wxTextCtrl* textBoxHostIP;
		wxStaticText* labelHostPort;
		wxTextCtrl* textBoxHostPort;
		
		wxStdDialogButtonSizer* dialogButtons;
		wxButton* dialogButtonsOK;
		wxButton* dialogButtonsCancel;
		
		// Virtual event handlers, overide them in your derived class
		virtual void onCheckBoxClicked( wxCommandEvent& event ){ event.Skip(); }
		virtual void buttonCancel_Clicked( wxCommandEvent& event ){ event.Skip(); }
		virtual void buttonOK_Clicked( wxCommandEvent& event ){ event.Skip(); }
		
	
	public:
		NetplayDialogBase( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("PCSX2 Netplay"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 287,263 ), long style = wxCAPTION|wxCLOSE_BOX|wxSTAY_ON_TOP|wxSYSTEM_MENU );
		~NetplayDialogBase();
	
};

#endif //__NetplayDialogBase__
