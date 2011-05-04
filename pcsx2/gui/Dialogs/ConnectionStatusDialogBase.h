///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Sep  8 2010)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#ifndef __ConnectionStatusDialogBase__
#define __ConnectionStatusDialogBase__

#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class ConnectionStatusDialogBase
///////////////////////////////////////////////////////////////////////////////
class ConnectionStatusDialogBase : public wxDialog 
{
	private:
	
	protected:
		wxStaticText* _statusText;
		wxStdDialogButtonSizer* _dialogButtons;
		wxButton* _dialogButtonsOK;
		wxButton* _dialogButtonsCancel;
		
		// Virtual event handlers, overide them in your derived class
		virtual void ButtonCancelClicked( wxCommandEvent& event ) { event.Skip(); }
		virtual void ButtonOKClicked( wxCommandEvent& event ) { event.Skip(); }
		
	
	public:
		
		ConnectionStatusDialogBase( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("Connection status"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 265,89 ), long style = wxDEFAULT_DIALOG_STYLE );
		~ConnectionStatusDialogBase();
	
};

#endif //__ConnectionStatusDialogBase__
