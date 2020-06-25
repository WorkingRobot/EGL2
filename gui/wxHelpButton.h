#pragma once

#include <wx/wxprec.h>
#include <wx/bmpbuttn.h>

class wxHelpButton : public wxBitmapButton {
public:
    wxHelpButton();

    wxHelpButton(wxWindow* parent, wxWindowID id, const wxString& description);

private:
    wxString clickData;

    void _OnClick(wxCommandEvent& evt);
};