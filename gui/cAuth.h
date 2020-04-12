#pragma once

#include "cMain.h"

#include "wxModalWindow.h"

#include <wx/wx.h>
#include <wx/hyperlink.h>


class cAuth : public wxModalWindow
{
public:
	cAuth(cMain* main);
	~cAuth();

	wxString& GetCode();

protected:
	wxPanel* panel = nullptr;

	wxStaticText* codeLabel = nullptr;
	wxTextCtrl* codeInput = nullptr;
	wxButton* codeLink = nullptr;
	wxButton* codeSubmit = nullptr;
	wxStaticText* codeInfo = nullptr;
	wxStaticBoxSizer* codeSecBox = nullptr;
	wxStaticText* codeSecInfo = nullptr;

	wxString returnValue;

	void OnGetCodeClicked(wxCommandEvent& evt);
	void OnSubmitClicked(wxCommandEvent& evt);
};

